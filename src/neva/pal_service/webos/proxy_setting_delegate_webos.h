// Copyright 2024 LG Electronics, Inc.
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

#ifndef NEVA_PAL_SERVICE_WEBOS_PROXY_SETTING_DELEGATE_WEBOS_H_
#define NEVA_PAL_SERVICE_WEBOS_PROXY_SETTING_DELEGATE_WEBOS_H_

#include "base/memory/weak_ptr.h"
#include "neva/pal_service/luna/luna_client.h"
#include "neva/pal_service/public/proxy_setting_delegate.h"

namespace content {
class ContentBrowserClient;
}

namespace pal {
namespace webos {

class ProxySettingDelegateWebos : public ProxySettingDelegate {
 public:
  ProxySettingDelegateWebos();
  ~ProxySettingDelegateWebos();

  void ObserveSystemProxySetting(
      content::ContentBrowserClient* content_browser_client) override;
  const content::ProxySettings GetProxySetting() override;

 private:
  ProxySettingDelegateWebos(const ProxySettingDelegateWebos&) = delete;
  ProxySettingDelegateWebos& operator=(const ProxySettingDelegateWebos&) =
      delete;

  void InitLunaServiceClient();
  void SubscribeProxyInforChange();
  bool GetProxyInfoFromSystemEnvironments();
  void GetProxyInfoFromSettingsServiceCallback(luna::Client::ResponseStatus,
                                               unsigned,
                                               const std::string& response);

  content::ContentBrowserClient* content_browser_client_ = nullptr;
  std::unique_ptr<luna::Client> luna_service_client_;
  bool is_use_system_enviroment_config_ = false;
  unsigned proxy_subscribe_key_;
  content::ProxySettings system_enviroments_proxy_info_;
  content::ProxySettings settings_service_proxy_info_;
  base::WeakPtrFactory<ProxySettingDelegateWebos> weak_factory_{this};
};

}  // namespace webos
}  // namespace pal

#endif  // NEVA_PAL_SERVICE_WEBOS_PROXY_SETTING_DELEGATE_WEBOS_H_