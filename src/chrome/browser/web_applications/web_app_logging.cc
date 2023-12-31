// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_logging.h"

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "net/http/http_status_code.h"

namespace web_app {
namespace {

bool IsEmptyIconBitmapsForIconUrl(const IconsMap& icons_map,
                                  const GURL& icon_url) {
  IconsMap::const_iterator iter = icons_map.find(icon_url);
  if (iter == icons_map.end())
    return true;

  const std::vector<SkBitmap>& icon_bitmaps = iter->second;
  if (icon_bitmaps.empty())
    return true;

  for (const SkBitmap& icon_bitmap : icon_bitmaps) {
    if (!icon_bitmap.isNull() && !icon_bitmap.drawsNothing())
      return false;
  }

  return true;
}

}  // namespace

InstallErrorLogEntry::InstallErrorLogEntry(
    bool background_installation,
    webapps::WebappInstallSource install_surface)
    : background_installation_(background_installation),
      install_surface_(install_surface) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo))
    error_dict_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
}

InstallErrorLogEntry::~InstallErrorLogEntry() = default;

base::Value InstallErrorLogEntry::TakeErrorDict() {
  DCHECK(error_dict_);
  base::Value error_dict = std::move(*error_dict_);
  error_dict_->DictClear();
  return error_dict;
}

void InstallErrorLogEntry::LogUrlLoaderError(const char* stage,
                                             const std::string& url,
                                             WebAppUrlLoader::Result result) {
  if (!error_dict_)
    return;

  base::Value url_loader_error(base::Value::Type::DICTIONARY);

  url_loader_error.SetStringKey("WebAppUrlLoader::Result",
                                ConvertUrlLoaderResultToString(result));

  LogErrorObject(stage, url, std::move(url_loader_error));
}

void InstallErrorLogEntry::LogExpectedAppIdError(const char* stage,
                                                 const std::string& url,
                                                 const AppId& app_id,
                                                 const AppId& expected_app_id) {
  if (!error_dict_)
    return;

  base::Value expected_app_id_error(base::Value::Type::DICTIONARY);
  expected_app_id_error.SetStringKey("expected_app_id", expected_app_id);
  expected_app_id_error.SetStringKey("app_id", app_id);

  LogErrorObject(stage, url, std::move(expected_app_id_error));
}

void InstallErrorLogEntry::LogDownloadedIconsErrors(
    const WebAppInstallInfo& web_app_info,
    IconsDownloadedResult icons_downloaded_result,
    const IconsMap& icons_map,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (!error_dict_)
    return;

  base::Value icon_errors(base::Value::Type::DICTIONARY);
  {
    // Reports errors only, omits successful entries.
    base::Value icons_http_errors(base::Value::Type::LIST);

    for (const auto& url_and_http_code : icons_http_results) {
      const GURL& icon_url = url_and_http_code.first;
      int http_status_code = url_and_http_code.second;
      const char* http_code_desc = net::GetHttpReasonPhrase(
          static_cast<net::HttpStatusCode>(http_status_code));

      // If the SkBitmap for`icon_url` is missing in `icons_map` then we report
      // this miss as an error, even for net::HttpStatusCode::HTTP_OK.
      if (IsEmptyIconBitmapsForIconUrl(icons_map, icon_url)) {
        base::Value icon_http_error(base::Value::Type::DICTIONARY);

        icon_http_error.SetStringKey("icon_url", icon_url.spec());
        icon_http_error.SetIntKey("http_status_code", http_status_code);
        icon_http_error.SetStringKey("http_code_desc", http_code_desc);

        icons_http_errors.Append(std::move(icon_http_error));
      }
    }

    if (icons_downloaded_result != IconsDownloadedResult::kCompleted ||
        !icons_http_errors.GetListDeprecated().empty()) {
      icon_errors.SetStringKey(
          "icons_downloaded_result",
          IconsDownloadedResultToString(icons_downloaded_result));
    }

    if (!icons_http_errors.GetListDeprecated().empty())
      icon_errors.SetKey("icons_http_results", std::move(icons_http_errors));
  }

  if (web_app_info.is_generated_icon)
    icon_errors.SetBoolKey("is_generated_icon", true);

  if (!icon_errors.DictEmpty()) {
    LogErrorObject("OnIconsRetrieved", web_app_info.start_url.spec(),
                   std::move(icon_errors));
  }
}

void InstallErrorLogEntry::LogHeaderIfLogEmpty(const std::string& url) {
  if (!error_dict_ || !error_dict_->DictEmpty())
    return;

  error_dict_->SetStringKey("!url", url);
  error_dict_->SetIntKey("install_surface", static_cast<int>(install_surface_));
  error_dict_->SetBoolKey("background_installation", background_installation_);
  error_dict_->SetKey("stages", base::Value(base::Value::Type::LIST));
}

void InstallErrorLogEntry::LogErrorObject(const char* stage,
                                          const std::string& url,
                                          base::Value object) {
  if (!error_dict_)
    return;

  LogHeaderIfLogEmpty(url);

  object.SetStringKey("!stage", stage);
  error_dict_->FindKey("stages")->Append(std::move(object));
}

}  // namespace web_app
