// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_app/media_app_guest_ui_config.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeMediaAppGuestUIDelegate::ChromeMediaAppGuestUIDelegate() = default;

void ChromeMediaAppGuestUIDelegate::PopulateLoadTimeData(
    content::WebUI* web_ui,
    content::WebUIDataSource* source) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* pref_service = profile->GetPrefs();
  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  bool photos_installed = false;
  auto photos_version = base::Version();
  app_registry_cache.ForOneApp(
      arc::kGooglePhotosAppId,
      [&photos_installed, &photos_version](const apps::AppUpdate& update) {
        photos_installed = apps_util::IsInstalled(update.Readiness());
        photos_version = base::Version(update.Version());
      });

  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  source->AddBoolean("pdfReadonly",
                     !pref_service->GetBoolean(prefs::kPdfAnnotationsEnabled));
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("colorThemes",
                     chromeos::features::IsDarkLightModeEnabled());
  base::Version min_photos_version_for_image(
      base::GetFieldTrialParamValueByFeature(
          chromeos::features::kMediaAppPhotosIntegrationImage,
          "minPhotosVersionForImage"));
  base::Version min_photos_version_for_video(
      base::GetFieldTrialParamValueByFeature(
          chromeos::features::kMediaAppPhotosIntegrationVideo,
          "minPhotosVersionForVideo"));
  bool suitable_photos_version = photos_installed && photos_version.IsValid();
  bool available_for_image = suitable_photos_version &&
                             min_photos_version_for_image.IsValid() &&
                             photos_version >= min_photos_version_for_image;
  bool available_for_video = suitable_photos_version &&
                             min_photos_version_for_video.IsValid() &&
                             photos_version >= min_photos_version_for_video;
  source->AddBoolean("photosAvailableForImage", available_for_image);
  source->AddBoolean("photosAvailableForVideo", available_for_video);
  source->AddBoolean("photosIntegrationImage",
                     base::FeatureList::IsEnabled(
                         chromeos::features::kMediaAppPhotosIntegrationImage));
  source->AddBoolean("photosIntegrationVideo",
                     base::FeatureList::IsEnabled(
                         chromeos::features::kMediaAppPhotosIntegrationVideo));
  bool enable_color_picker_improvements =
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppCustomColors);
  source->AddBoolean("recentColorPalette", enable_color_picker_improvements);
  source->AddBoolean("customColorSelector", enable_color_picker_improvements);
  source->AddBoolean("flagsMenu", channel != version_info::Channel::BETA &&
                                      channel != version_info::Channel::STABLE);
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
}

MediaAppGuestUIConfig::MediaAppGuestUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::kChromeUIMediaAppHost) {}

MediaAppGuestUIConfig::~MediaAppGuestUIConfig() = default;

std::unique_ptr<content::WebUIController>
MediaAppGuestUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  ChromeMediaAppGuestUIDelegate delegate;
  return std::make_unique<ash::MediaAppGuestUI>(web_ui, &delegate);
}
