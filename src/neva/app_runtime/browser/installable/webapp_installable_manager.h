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

#ifndef NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_
#define NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_

#include <memory>

#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"

struct WebAppInstallInfo;

namespace neva_app_runtime {

class WebAppInstallableManager {
 public:
  WebAppInstallableManager();
  virtual ~WebAppInstallableManager();

  using CheckInstallabilityCallback =
      base::OnceCallback<void(bool installable, bool installed)>;
  void CheckInstallability(content::WebContents* web_contents,
                           CheckInstallabilityCallback callback);

  using InstallWebAppCallback = base::OnceCallback<void(bool success)>;
  void InstallWebApp(content::WebContents* web_contents,
                     InstallWebAppCallback callback);
  void MaybeUpdate(content::WebContents* web_contents);

 private:
  void OnCheckInstallability(CheckInstallabilityCallback callback,
                             blink::mojom::ManifestPtr opt_manifest,
                             const GURL& manifest_url,
                             bool valid_manifest_for_web_app,
                             bool is_installable);
  void OnIsWebAppForUrlInstallability(bool is_installable,
                                      CheckInstallabilityCallback callback,
                                      bool is_installed);
  void OnWebAppForUrlisUpdate(content::WebContents* web_contents,
                              std::unique_ptr<WebAppInstallInfo> web_app_info,
                              bool is_installed);
  void OnIconsDownloaded(InstallWebAppCallback callback,
                         std::unique_ptr<WebAppInstallInfo> web_app_info,
                         web_app::IconsDownloadedResult result,
                         IconsMap icons_map,
                         DownloadedIconsHttpResults icons_http_results);
  void OnDidGetManifest(content::WebContents* web_contents,
                        InstallWebAppCallback callback,
                        const GURL& manifest_url,
                        blink::mojom::ManifestPtr manifest);
  std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo> ConvertAppInfo(
      const WebAppInstallInfo* web_app_info);
  void OnManifestForUpdate(content::WebContents* web_contents,
                           blink::mojom::ManifestPtr opt_manifest,
                           const GURL& manifest_url,
                           bool valid_manifest_for_web_app,
                           bool is_installable);
  void OnShouldAppForURLBeUpdated(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      bool should_update);
  void OnIconsDownloadedForUpdate(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      web_app::IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnIsInfoChanged(
      std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
          new_delegate_info,
      bool value,
      const std::string& version);

  std::unique_ptr<web_app::WebAppDataRetriever> data_retriever_;
  std::unique_ptr<pal::WebAppInstallableDelegate> pal_installable_delegate_;
  base::WeakPtrFactory<WebAppInstallableManager> weak_factory_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_
