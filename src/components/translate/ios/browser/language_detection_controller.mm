// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/language_detection_controller.h"

#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/prefs/pref_member.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "components/translate/ios/browser/string_clipping_util.h"
#import "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "net/http/http_response_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {
// Name for the UMA metric used to track text extraction time.
const char kTranslateCaptureText[] = "Translate.CaptureText";
// Name for the UMA metric used to track language detection evaluation duration.
const char kTranslateLanguageDetectionTFLiteModelEvaluationDuration[] =
    "Translate.LanguageDetection.TFLiteModelEvaluationDuration";
// Prefix for the language detection javascript commands. Must be kept in sync
// with language_detection.js.
const char kCommandPrefix[] = "languageDetection";

// The old CLD model version.
const char kCLDModelVersion[] = "CLD3";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LanguageDetectionMethod {
  kTFLiteModelUsed = 0,
  kTFLiteModelUnavailable = 1,
  kTFLiteModelDisabled = 2,
  kTFLiteModelIgnored = 3,
  kMaxValue = kTFLiteModelIgnored
};

enum class LanguageDetectionComparison {
  kTFLiteModelOnly = 0,
  kCLD3ModelOnly = 1,
  kBothModelFailed = 2,
  kBothModelAgree = 3,
  kBothModelDisagree = 4,
  kMaxValue = kBothModelDisagree
};

void ComparePageLanguageDetection(const std::string& tflite_language,
                                  const std::string& cld3_language) {
  bool tflite_failed = tflite_language.empty() ||
                       tflite_language == translate::kUnknownLanguageCode;
  bool cld3_failed =
      cld3_language.empty() || cld3_language == translate::kUnknownLanguageCode;

  if (tflite_failed) {
    if (cld3_failed) {
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionComparison",
          LanguageDetectionComparison::kBothModelFailed);
    } else {
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionComparison",
          LanguageDetectionComparison::kCLD3ModelOnly);
    }
  } else {
    if (cld3_failed) {
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionComparison",
          LanguageDetectionComparison::kTFLiteModelOnly);
    } else if (cld3_language == tflite_language) {
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionComparison",
          LanguageDetectionComparison::kBothModelAgree);
    } else {
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionComparison",
          LanguageDetectionComparison::kBothModelDisagree);
    }
  }
}
}

// Note: This should stay in sync with the constant in language_detection.js.
const size_t kMaxIndexChars = 65535;

LanguageDetectionController::LanguageDetectionController(
    web::WebState* web_state,
    LanguageDetectionModel* language_detection_model,
    PrefService* prefs)
    : web_state_(web_state),
      language_detection_model_(language_detection_model),
      weak_method_factory_(this) {
  DCHECK(web_state_);

  translate_enabled_.Init(prefs::kOfferTranslateEnabled, prefs);
  // Attempt to detect language since preloaded tabs will not execute
  // WebStateObserver::PageLoaded.
  StartLanguageDetection();
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::BindRepeating(&LanguageDetectionController::OnTextCaptured,
                          base::Unretained(this)),
      kCommandPrefix);
}

LanguageDetectionController::~LanguageDetectionController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void LanguageDetectionController::StartLanguageDetection() {
  if (!translate_enabled_.GetValue())
    return;  // Translate disabled in preferences.
  DCHECK(web_state_);
  const GURL& url = web_state_->GetVisibleURL();
  if (!web::UrlHasWebScheme(url) || !web_state_->ContentIsHTML())
    return;

  web::WebFrame* web_frame = web::GetMainFrame(web_state_);
  if (!web_frame) {
    return;
  }

  web_frame->CallJavaScriptFunction("languageDetection.detectLanguage", {});
}

void LanguageDetectionController::OnTextCaptured(const base::Value& command,
                                                 const GURL& url,
                                                 bool user_is_interacting,
                                                 web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame()) {
    // Translate is only supported on main frame.
    return;
  }
  const std::string* text_captured_command = command.FindStringKey("command");
  if (!text_captured_command ||
      *text_captured_command != "languageDetection.textCaptured") {
    return;
  }
  absl::optional<bool> has_notranslate = command.FindBoolKey("hasNoTranslate");
  absl::optional<double> capture_text_time =
      command.FindDoubleKey("captureTextTime");
  const std::string* html_lang = command.FindStringKey("htmlLang");
  const std::string* http_content_language =
      command.FindStringKey("httpContentLanguage");
  if (!has_notranslate.has_value() || !capture_text_time.has_value() ||
      !html_lang || !http_content_language) {
    return;
  }

  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::Milliseconds(*capture_text_time));

  // If there is no language defined in httpEquiv, use the HTTP header.
  if (http_content_language->empty())
    http_content_language = &content_language_header_;

  sender_frame->CallJavaScriptFunction(
      "languageDetection.retrieveBufferedTextContent", {},
      base::BindRepeating(&LanguageDetectionController::OnTextRetrieved,
                          weak_method_factory_.GetWeakPtr(), *has_notranslate,
                          *http_content_language, *html_lang, url),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));
}

// Select the correct DeterminePageLanguage to call based on the feature flags.
std::string LanguageDetectionController::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const std::u16string& contents,
    std::string* model_detected_language,
    bool* is_model_reliable,
    float& model_reliability_score,
    std::string* detection_model_version) {
  if (IsTFLiteLanguageDetectionEnabled() && language_detection_model_ &&
      language_detection_model_->IsAvailable()) {
    base::ElapsedTimer timer;
    std::string tflite_language =
        language_detection_model_->DeterminePageLanguage(
            code, html_lang, contents, model_detected_language,
            is_model_reliable, model_reliability_score);
    base::UmaHistogramTimes(
        kTranslateLanguageDetectionTFLiteModelEvaluationDuration,
        timer.Elapsed());

    if (!IsTFLiteLanguageDetectionIgnoreEnabled()) {
      *detection_model_version = language_detection_model_->GetModelVersion();
      base::UmaHistogramEnumeration(
          "IOS.Translate.PageLoad.LanguageDetectionMethod",
          LanguageDetectionMethod::kTFLiteModelUsed);
      return tflite_language;
    }

    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelIgnored);
    std::string cld3_language = ::translate::DeterminePageLanguage(
        code, html_lang, contents, model_detected_language, is_model_reliable,
        model_reliability_score);

    ComparePageLanguageDetection(tflite_language, cld3_language);
    return cld3_language;
  }

  if (IsTFLiteLanguageDetectionEnabled()) {
    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelUnavailable);
  } else {
    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelDisabled);
  }
  return ::translate::DeterminePageLanguage(
      code, html_lang, contents, model_detected_language, is_model_reliable,
      model_reliability_score);
}

void LanguageDetectionController::OnTextRetrieved(
    const bool has_notranslate,
    const std::string& http_content_language,
    const std::string& html_lang,
    const GURL& url,
    const base::Value* text_content) {
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::u16string text = text_content && text_content->is_string()
                            ? base::UTF8ToUTF16(text_content->GetString())
                            : std::u16string();

  std::string detection_model_version = kCLDModelVersion;

  std::string language =
      DeterminePageLanguage(http_content_language, html_lang,
                            GetStringByClippingLastWord(text, kMaxIndexChars),
                            &model_detected_language, &is_model_reliable,
                            model_reliability_score, &detection_model_version);

  if (language.empty())
    return;  // No language detected.

  // Avoid an unnecessary copy of the full text content (which can be
  // ~64kB) until we need it on iOS (e.g. for the translate internals
  // page).
  LanguageDetectionDetails details;
  details.time = base::Time::Now();
  details.url = url;
  details.has_notranslate = has_notranslate;
  details.content_language = http_content_language;
  details.model_detected_language = model_detected_language;
  details.is_model_reliable = is_model_reliable;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  details.detection_model_version = detection_model_version;

  language::IOSLanguageDetectionTabHelper::FromWebState(web_state_)
      ->OnLanguageDetermined(details);
}

void LanguageDetectionController::ExtractContentLanguageHeader(
    net::HttpResponseHeaders* headers) {
  if (!headers) {
    content_language_header_.clear();
    return;
  }

  headers->GetNormalizedHeader("content-language", &content_language_header_);
  // Remove everything after the comma ',' if any.
  size_t comma_index = content_language_header_.find_first_of(',');
  if (comma_index != std::string::npos)
    content_language_header_.resize(comma_index);
}

// web::WebStateObserver implementation:

void LanguageDetectionController::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    StartLanguageDetection();
}

void LanguageDetectionController::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->IsSameDocument()) {
    StartLanguageDetection();
  } else {
    ExtractContentLanguageHeader(navigation_context->GetResponseHeaders());
  }
}

void LanguageDetectionController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

}  // namespace translate
