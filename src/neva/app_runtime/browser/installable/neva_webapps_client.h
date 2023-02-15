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

#ifndef NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_NEVA_WEBAPPS_CLIENT_H_
#define NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_NEVA_WEBAPPS_CLIENT_H_

#include "base/no_destructor.h"
#include "components/webapps/browser/webapps_client.h"

namespace webapps {
class AppBannerManager;
enum class InstallTrigger;
enum class WebappInstallSource;
}  // namespace webapps

namespace neva_app_runtime {

class NevaWebappsClient : public webapps::WebappsClient {
 public:
  ~NevaWebappsClient() override = default;

  static void Create();

  // webapps::WebappsClient
  security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) override;
  infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) override;
  webapps::WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      webapps::InstallTrigger trigger) override;
  webapps::AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) override;

 private:
  friend base::NoDestructor<NevaWebappsClient>;
  NevaWebappsClient() = default;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_NEVA_WEBAPPS_CLIENT_H_
