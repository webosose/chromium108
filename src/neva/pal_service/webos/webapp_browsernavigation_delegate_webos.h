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

#ifndef NEVA_PAL_SERVICE_WEBOS_WEBAPP_BROWSERNAVIGATION_DELEGATE_WEBOS_H_
#define NEVA_PAL_SERVICE_WEBOS_WEBAPP_BROWSERNAVIGATION_DELEGATE_WEBOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "neva/pal_service/luna/luna_client.h"
#include "neva/pal_service/public/webapp_browsernavigation_delegate.h"

namespace pal {
namespace webos {

class WebAppBrowserNavigationDelegateWebOS
    : public WebAppBrowserNavigationDelegate {
 public:
  WebAppBrowserNavigationDelegateWebOS();
  ~WebAppBrowserNavigationDelegateWebOS() override = default;
  WebAppBrowserNavigationDelegateWebOS(
      const WebAppBrowserNavigationDelegateWebOS&) = delete;
  WebAppBrowserNavigationDelegateWebOS& operator=(
      const WebAppBrowserNavigationDelegateWebOS&) = delete;

  static std::unique_ptr<luna::Client> InitLunaClient();

  void OpenUrlInBrowser(const std::string& url) override;

 private:
  void OnOpenUrlInBrowser(pal::luna::Client::ResponseStatus status,
                          unsigned token,
                          const std::string& json);

  std::unique_ptr<luna::Client> luna_client_;
  base::WeakPtrFactory<WebAppBrowserNavigationDelegateWebOS> weak_ptr_factory_;
};

}  // namespace webos
}  // namespace pal

#endif  // NEVA_PAL_SERVICE_WEBOS_WEBAPP_BROWSERNAVIGATION_DELEGATE_WEBOS_H_
