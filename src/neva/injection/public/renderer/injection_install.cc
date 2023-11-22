// Copyright 2019 LG Electronics, Inc.
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

#include "neva/injection/public/renderer/injection_install.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "neva/injection/public/common/webapi_names.h"

#include "neva/injection/public/renderer/cookiemanager_webapi.h"
#include "neva/injection/public/renderer/customuseragent_webapi.h"
#include "neva/injection/public/renderer/mediacapture_webapi.h"
#include "neva/injection/public/renderer/popupblocker_webapi.h"
#include "neva/injection/public/renderer/sitefilter_webapi.h"
#include "neva/injection/public/renderer/userpermission_webapi.h"
#if defined(USE_GAV)
#include "neva/injection/public/renderer/webosgavplugin_webapi.h"
#endif

#if defined(ENABLE_WEBOS_SERVICE_BRIDGE_WEBAPI)
#include "neva/injection/public/renderer/webosservicebridge_webapi.h"
#endif

#if defined(ENABLE_WEBOS_SYSTEM_WEBAPI)
#include "neva/injection/public/renderer/webossystem_webapi.h"
#endif

#if defined(ENABLE_SAMPLE_WEBAPI)
#include "neva/injection/public/renderer/sample_webapi.h"
#endif

#if defined(ENABLE_BROWSER_CONTROL_WEBAPI)
#include "neva/injection/public/renderer/browser_control_webapi.h"
#endif

#if defined(ENABLE_MEMORYMANAGER_WEBAPI)
#include "neva/injection/public/renderer/memorymanager_webapi.h"
#endif

#if defined(ENABLE_BROWSER_SHELL)
#include "neva/injection/public/renderer/browser_shell_ipc_webapi.h"
#include "neva/injection/public/renderer/browser_shell_webapi.h"
#endif

#if defined(ENABLE_NETWORK_ERROR_PAGE_CONTROLLER_WEBAPI)
#include "neva/injection/public/renderer/network_error_page_controller_webapi.h"
#endif

#if defined(USE_NEVA_CHROME_EXTENSIONS)
#include "neva/injection/public/renderer/chrome_extensions_webapi.h"
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

namespace injections {

bool GetInjectionInstallAPI(const std::string& name, InstallAPI* api) {
  DCHECK(api != nullptr);
#if defined(ENABLE_WEBOS_SYSTEM_WEBAPI)
  if ((name == webapi::kWebOSSystem) ||
      (name == webapi::kWebOSSystemObsolete)) {
    api->install_func = WebOSSystemWebAPI::Install;
    api->uninstall_func = WebOSSystemWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_WEBOS_SERVICE_BRIDGE_WEBAPI)
  if ((name == webapi::kWebOSServiceBridge) ||
      (name == webapi::kWebOSServiceBridgeObsolete)) {
    api->install_func = WebOSServiceBridgeWebAPI::Install;
    api->uninstall_func = WebOSServiceBridgeWebAPI::Uninstall;
    return true;
  }
#endif
  if (name == webapi::kCookieManager) {
    api->install_func = CookieManagerWebAPI::Install;
    api->uninstall_func = CookieManagerWebAPI::Uninstall;
    return true;
  }
  if (name == webapi::kMediaCapture) {
    api->install_func = MediaCaptureWebAPI::Install;
    api->uninstall_func = MediaCaptureWebAPI::Uninstall;
    return true;
  }
  if (name == webapi::kPopupBlocker) {
    api->install_func = PopupBlockerWebAPI::Install;
    api->uninstall_func = PopupBlockerWebAPI::Uninstall;
    return true;
  }
  if (name == webapi::kSiteFilter) {
    api->install_func = SiteFilterWebAPI::Install;
    api->uninstall_func = SiteFilterWebAPI::Uninstall;
    return true;
  }
  if (name == webapi::kUserPermission) {
    api->install_func = UserPermissionWebAPI::Install;
    api->uninstall_func = UserPermissionWebAPI::Uninstall;
    return true;
  }
  if (name == webapi::kCustomUserAgent) {
    api->install_func = CustomUserAgentWebAPI::Install;
    api->uninstall_func = CustomUserAgentWebAPI::Uninstall;
    return true;
  }
#if defined(USE_GAV)
  if (name == webapi::kWebOSGAV) {
    api->install_func = WebOSGAVWebAPI::Install;
    api->uninstall_func = WebOSGAVWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_SAMPLE_WEBAPI)
  if (name == webapi::kSample) {
    api->install_func = SampleWebAPI::Install;
    api->uninstall_func = SampleWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_MEMORYMANAGER_WEBAPI)
  if (name == webapi::kMemoryManager) {
    api->install_func = MemoryManagerWebAPI::Install;
    api->uninstall_func = MemoryManagerWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_BROWSER_CONTROL_WEBAPI)
  if (name == webapi::kBrowserControl) {
    api->install_func = BrowserControlWebAPI::Install;
    api->uninstall_func = BrowserControlWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_BROWSER_SHELL)
  if (name == webapi::kBrowserShell) {
    api->install_func = BrowserShellWebAPI::Install;
    api->uninstall_func = BrowserShellWebAPI::Uninstall;
    return true;
  }

  if (name == webapi::kBrowserShellIpc) {
    api->install_func = BrowserShellIpcWebAPI::Install;
    api->uninstall_func = BrowserShellIpcWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(ENABLE_NETWORK_ERROR_PAGE_CONTROLLER_WEBAPI)
  if (name == webapi::kNetworkErrorPage) {
    api->install_func = NetworkErrorPageControllerWebAPI::Install;
    api->uninstall_func = NetworkErrorPageControllerWebAPI::Uninstall;
    return true;
  }
#endif
#if defined(USE_NEVA_CHROME_EXTENSIONS)
  if (name == webapi::kChromeExtensions) {
    api->install_func = ChromeExtensionsWebAPI::Install;
    api->uninstall_func = ChromeExtensionsWebAPI::Uninstall;
    return true;
  }
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

  return false;
}

}  // namespace injections
