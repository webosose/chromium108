// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/ios/browser/language_detection_controller.h"
#include "components/translate/ios/browser/translate_controller.h"
#include "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}

namespace translate {

class LanguageDetectionModelService;
class TranslateManager;

// Content implementation of TranslateDriver.
class IOSTranslateDriver
    : public TranslateDriver,
      public TranslateController::Observer,
      public web::WebStateObserver,
      public language::IOSLanguageDetectionTabHelper::Observer {
 public:
  IOSTranslateDriver(
      web::WebState* web_state,
      TranslateManager* translate_manager,
      LanguageDetectionModelService* language_detection_model_service);

  IOSTranslateDriver(const IOSTranslateDriver&) = delete;
  IOSTranslateDriver& operator=(const IOSTranslateDriver&) = delete;

  ~IOSTranslateDriver() override;

  LanguageDetectionController* language_detection_controller() {
    return language_detection_controller_.get();
  }

  void OnLanguageModelFileAvailabilityChanged(bool available);

  // web::WebStateObserver methods.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // language::IOSLanguageDetectionTabHelper::Observer.
  void OnLanguageDetermined(const LanguageDetectionDetails& details) override;
  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override;

  // TranslateDriver methods.
  void OnIsPageTranslatedChanged() override;
  void OnTranslateEnabledChanged() override;
  bool IsLinkNavigation() override;
  void TranslatePage(int page_seq_no,
                     const std::string& translate_script,
                     const std::string& source_lang,
                     const std::string& target_lang) override;
  void RevertTranslation(int page_seq_no) override;
  bool IsIncognito() override;
  const std::string& GetContentsMimeType() override;
  const GURL& GetLastCommittedURL() override;
  const GURL& GetVisibleURL() override;
  ukm::SourceId GetUkmSourceId() override;
  bool HasCurrentPage() override;
  void OpenUrlInNewTab(const GURL& url) override;

 private:
  // Called when the translation was successful.
  void TranslationDidSucceed(const std::string& source_lang,
                             const std::string& target_lang,
                             int page_seq_no,
                             const std::string& original_page_language,
                             double translation_time);

  // Returns true if the user has not navigated away and the the page is not
  // being destroyed.
  bool IsPageValid(int page_seq_no) const;

  // TranslateController::Observer methods.
  void OnTranslateScriptReady(TranslateErrors error_type,
                              double load_time,
                              double ready_time) override;
  void OnTranslateComplete(TranslateErrors error_type,
                           const std::string& source_language,
                           double translation_time) override;

  // Stops observing |web_state_| and sets it to null.
  void StopObservingWebState();

  // Stops observing the IOSLanguageDetectionTabHelper instance associated with
  // |web_state_|.
  void StopObservingIOSLanguageDetectionTabHelper();

  // The WebState this instance is observing.
  web::WebState* web_state_ = nullptr;

  base::WeakPtr<TranslateManager> translate_manager_;
  std::unique_ptr<LanguageDetectionController> language_detection_controller_;

  LanguageDetectionModelService* language_detection_model_service_ = nullptr;

  // An ever-increasing sequence number of the current page, used to match up
  // translation requests with responses.
  // This matches the similar field in TranslateAgent in the renderer on other
  // platforms.
  int page_seq_no_;

  // When a translation is in progress, its page sequence number is stored in
  // |pending_page_seq_no_|.
  int pending_page_seq_no_;

  // Parameters of the current translation.
  std::string source_language_;
  std::string target_language_;

  base::WeakPtrFactory<IOSTranslateDriver> weak_ptr_factory_{this};
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_IOS_TRANSLATE_DRIVER_H_
