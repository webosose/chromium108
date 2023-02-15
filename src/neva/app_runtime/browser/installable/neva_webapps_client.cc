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

#include "neva/app_runtime/browser/installable/neva_webapps_client.h"

#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace neva_app_runtime {

// static
void NevaWebappsClient::Create() {
  static base::NoDestructor<NevaWebappsClient> instance;
}

security_state::SecurityLevel NevaWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  // Security check is a simplified version comparing to ChromeWebappsClient.
  return security_state::GetSecurityLevel(
      *security_state::GetVisibleSecurityState(web_contents), false);
}

infobars::ContentInfoBarManager*
NevaWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return nullptr;
}

webapps::WebappInstallSource NevaWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    webapps::InstallTrigger trigger) {
  return webapps::WebappInstallSource::MENU_BROWSER_TAB;
}

webapps::AppBannerManager* NevaWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
  return nullptr;
}

}  // namespace neva_app_runtime
