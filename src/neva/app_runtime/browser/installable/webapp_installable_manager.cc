// Copyright 2023 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "neva/app_runtime/browser/installable/webapp_installable_manager.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/web_contents.h"
#include "neva/app_runtime/browser/installable/neva_webapps_client.h"
#include "neva/pal_service/pal_platform_factory.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace neva_app_runtime {

WebAppInstallableManager::WebAppInstallableManager()
    : data_retriever_(std::make_unique<web_app::WebAppDataRetriever>()),
      pal_installable_delegate_(
          pal::PlatformFactory::Get()->CreateWebAppInstallableDelegate()),
      weak_factory_(this) {
  NevaWebappsClient::Create();
}

WebAppInstallableManager::~WebAppInstallableManager() {}

void WebAppInstallableManager::CheckInstallability(
    content::WebContents* web_contents,
    CheckInstallabilityCallback callback) {
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents, true,
      base::BindOnce(&WebAppInstallableManager::OnCheckInstallability,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppInstallableManager::OnCheckInstallability(
    CheckInstallabilityCallback callback,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  VLOG(1) << std::boolalpha << __func__
          << "() valid_manifest_for_web_app=" << valid_manifest_for_web_app
          << " installable=" << is_installable;

  if (!opt_manifest)
    return std::move(callback).Run(false, false);

  if (valid_manifest_for_web_app) {
    WebAppInstallInfo web_app_info;
    web_app::UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                          &web_app_info);
    pal_installable_delegate_->IsWebAppForUrlInstalled(
        web_app_info.start_url,
        base::BindOnce(
            &WebAppInstallableManager::OnIsWebAppForUrlInstallability,
            weak_factory_.GetWeakPtr(), is_installable, std::move(callback)));
  }
}

void WebAppInstallableManager::OnIsWebAppForUrlInstallability(
    bool is_installable,
    CheckInstallabilityCallback callback,
    bool is_installed) {
  std::move(callback).Run(is_installable, is_installed);
}

void WebAppInstallableManager::InstallWebApp(content::WebContents* web_contents,
                                             InstallWebAppCallback callback) {
  if (!web_contents)
    return std::move(callback).Run(false);

  web_contents->GetPrimaryPage().GetManifest(base::BindOnce(
      &WebAppInstallableManager::OnDidGetManifest, weak_factory_.GetWeakPtr(),
      web_contents, std::move(callback)));
}

void WebAppInstallableManager::OnDidGetManifest(
    content::WebContents* web_contents,
    InstallWebAppCallback callback,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  if (!manifest || manifest_url.is_empty() ||
      blink::IsEmptyManifest(manifest)) {
    std::move(callback).Run(false);
    return;
  }

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app::UpdateWebAppInfoFromManifest(*manifest, manifest_url,
                                        web_app_info.get());
  data_retriever_->GetIcons(
      web_contents, web_app::GetValidIconUrlsToDownload(*web_app_info), true,
      base::BindOnce(&WebAppInstallableManager::OnIconsDownloaded,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(web_app_info)));
}

void WebAppInstallableManager::OnIconsDownloaded(
    InstallWebAppCallback callback,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    web_app::IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  web_app::PopulateProductIcons(web_app_info.get(), &icons_map);
  web_app::PopulateOtherIcons(web_app_info.get(), icons_map);

  auto delegate_info = ConvertAppInfo(web_app_info.get());
  bool install_result =
      pal_installable_delegate_->SaveArtifacts(delegate_info.get());

  std::move(callback).Run(install_result);
}

void WebAppInstallableManager::UpdateApp() {
  pal_installable_delegate_->UpdateApp();
}

// Update
void WebAppInstallableManager::MaybeUpdate(content::WebContents* web_contents) {
  VLOG(1) << "Begin update steps of PWA app";
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents, true,
      base::BindOnce(&WebAppInstallableManager::OnManifestForUpdate,
                     weak_factory_.GetWeakPtr(), web_contents));
}

void WebAppInstallableManager::OnManifestForUpdate(
    content::WebContents* web_contents,
    blink::mojom::ManifestPtr manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  VLOG(1) << "manifest_url: " << manifest_url
          << ", valid_manifest_for_web_app: " << valid_manifest_for_web_app
          << ", is_installable: " << is_installable;
  if (!manifest || !valid_manifest_for_web_app || !is_installable)
    return;

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app::UpdateWebAppInfoFromManifest(*manifest, manifest_url,
                                        web_app_info.get());

  pal_installable_delegate_->IsWebAppForUrlInstalled(
      web_app_info->start_url,
      base::BindOnce(&WebAppInstallableManager::OnWebAppForUrlisUpdate,
                     weak_factory_.GetWeakPtr(), web_contents,
                     std::move(web_app_info)));
}

void WebAppInstallableManager::OnWebAppForUrlisUpdate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    bool is_installed) {
  if (is_installed) {
    pal_installable_delegate_->ShouldAppForURLBeUpdated(
        web_app_info->start_url,
        base::BindOnce(&WebAppInstallableManager::OnShouldAppForURLBeUpdated,
                       weak_factory_.GetWeakPtr(), web_contents,
                       std::move(web_app_info)));
  } else {
    VLOG(1) << "Do not update because the app is not installed";
  }
}

void WebAppInstallableManager::OnShouldAppForURLBeUpdated(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    bool should_update) {
  if (!should_update) {
    VLOG(1) << "The app should not be updated now";
    return;
  }

  // Icons
  auto url = web_app::GetValidIconUrlsToDownload(*web_app_info);
  data_retriever_->GetIcons(
      web_contents, url, true,
      base::BindOnce(&WebAppInstallableManager::OnIconsDownloadedForUpdate,
                     weak_factory_.GetWeakPtr(), std::move(web_app_info)));
}

void WebAppInstallableManager::OnIconsDownloadedForUpdate(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    web_app::IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  web_app::PopulateProductIcons(web_app_info.get(), &icons_map);
  web_app::PopulateOtherIcons(web_app_info.get(), icons_map);

  pal_installable_delegate_->IsInfoChanged(
      ConvertAppInfo(web_app_info.get()),
      base::BindOnce(&WebAppInstallableManager::OnIsInfoChanged,
                     weak_factory_.GetWeakPtr(),
                     ConvertAppInfo(web_app_info.get())));
}

void WebAppInstallableManager::OnIsInfoChanged(
    std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
        new_delegate_info,
    bool value,
    const std::string& version) {
  if (value) {
    VLOG(1) << "Proceed with updating the app";
    new_delegate_info->set_version(version);
    bool install_result =
        pal_installable_delegate_->SaveArtifacts(new_delegate_info.get(), true);
    VLOG(1) << "The app update install_result: " << install_result;
  } else {
    VLOG(1) << "Do not update the app because resources are not changed";
  }
}

std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
WebAppInstallableManager::ConvertAppInfo(
    const WebAppInstallInfo* web_app_info) {
  return pal_installable_delegate_->GenerateAppInfo(
      base::UTF16ToUTF8(web_app_info->title), web_app_info->icon_bitmaps.any,
      web_app_info->start_url, web_app_info->background_color);
}
}  // namespace neva_app_runtime
