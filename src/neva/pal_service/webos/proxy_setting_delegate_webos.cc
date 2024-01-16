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

#include "neva/pal_service/webos/proxy_setting_delegate_webos.h"

#include "base/environment.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "neva/pal_service/luna/luna_names.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
const char kWebosProxy[] = "com.webos.settingsservice.client-webos.proxy";
const char kGetMethodName[] = "getSystemSettings";
const char kSubscribe[] = "subscribe";
const char kCategory[] = "category";

std::string ValueOrEmpty(const std::string* input) {
  return input ? *input : std::string();
}
}  // namespace

namespace pal {
namespace webos {

ProxySettingDelegateWebos::ProxySettingDelegateWebos() = default;

ProxySettingDelegateWebos::~ProxySettingDelegateWebos() {
  if (proxy_subscribe_key_)
    luna_service_client_->Unsubscribe(proxy_subscribe_key_);
}

void ProxySettingDelegateWebos::InitLunaServiceClient() {
  if (luna_service_client_) {
    return;
  }

  pal::luna::Client::Params params;
  params.name = std::string(kWebosProxy);
  luna_service_client_ = luna::CreateClient(params);
}

void ProxySettingDelegateWebos::SubscribeProxyInforChange() {
  base::Value::Dict proxy_list_root;
  proxy_list_root.Set(kCategory, "commercial");
  proxy_list_root.Set(kSubscribe, true);

  std::string proxy_payload;
  if (!base::JSONWriter::Write(proxy_list_root, &proxy_payload)) {
    LOG(ERROR) << __func__ << " Failed to write proxy payload";
    return;
  }

  InitLunaServiceClient();
  luna_service_client_->Subscribe(
      luna::GetServiceURI(luna::service_uri::kSettings, kGetMethodName),
      proxy_payload,
      base::BindRepeating(
          &ProxySettingDelegateWebos::GetProxyInfoFromSettingsServiceCallback,
          weak_factory_.GetWeakPtr()),
      std::string("{}"), &proxy_subscribe_key_);
}

void ProxySettingDelegateWebos::ObserveSystemProxySetting(
    content::ContentBrowserClient* content_browser_client) {
  content_browser_client_ = content_browser_client;
  if (GetProxyInfoFromSystemEnvironments()) {
    is_use_system_enviroment_config_ = true;
    content_browser_client_->SetProxyServer(system_enviroments_proxy_info_);
  }
  SubscribeProxyInforChange();
}

void ProxySettingDelegateWebos::GetProxyInfoFromSettingsServiceCallback(
    luna::Client::ResponseStatus,
    unsigned,
    const std::string& response) {
  if (is_use_system_enviroment_config_) {
    is_use_system_enviroment_config_ = false;
    return;
  }

  absl::optional<base::Value> response_value = base::JSONReader::Read(response);
  base::Value::Dict* dict = response_value->GetIfDict();
  if (!dict)
    return;

  if (!dict->FindBool("returnValue").value_or(false))
    return;

  auto* setting_dict = dict->FindDict("settings");
  if (!setting_dict)
    return;

  content::ProxySettings proxy_settings;
  std::string enabled = ValueOrEmpty(setting_dict->FindString("proxyEnable"));
  proxy_settings.enabled = enabled == "on";
  proxy_settings.ip =
      ValueOrEmpty(setting_dict->FindString("proxySingleAddress"));
  proxy_settings.port =
      ValueOrEmpty(setting_dict->FindString("proxySinglePort"));
  proxy_settings.mode = ValueOrEmpty(setting_dict->FindString("proxyMode"));
  proxy_settings.scheme = ValueOrEmpty(setting_dict->FindString("proxyScheme"));
  proxy_settings.username =
      ValueOrEmpty(setting_dict->FindString("proxySingleUsername"));
  proxy_settings.password =
      ValueOrEmpty(setting_dict->FindString("proxySinglePassword"));
  proxy_settings.bypass_list =
      ValueOrEmpty(setting_dict->FindString("proxyBypassList"));
  settings_service_proxy_info_ = proxy_settings;
  content_browser_client_->SetProxyServer(settings_service_proxy_info_);
}

bool ProxySettingDelegateWebos::GetProxyInfoFromSystemEnvironments() {
  auto env = base::Environment::Create();
  std::string proxy_single_address;
  std::string proxy_single_port;
  if (!env->GetVar("PROXY_HOST", &proxy_single_address) ||
      !env->GetVar("PROXY_PORT", &proxy_single_port))
    return false;

  content::ProxySettings proxy_settings;
  proxy_settings.ip = proxy_single_address;
  proxy_settings.port = proxy_single_port;
  proxy_settings.enabled = true;
  std::string scheme;
  if (env->GetVar("PROXY_SCHEME", &scheme))
    proxy_settings.scheme = scheme;
  std::string username;
  if (env->GetVar("PROXY_ID", &username))
    proxy_settings.username = username;
  std::string password;
  if (env->GetVar("PROXY_PW", &password))
    proxy_settings.password = password;
  system_enviroments_proxy_info_ = proxy_settings;
  return true;
}

const content::ProxySettings ProxySettingDelegateWebos::GetProxySetting() {
  if (settings_service_proxy_info_.enabled)
    return settings_service_proxy_info_;
  return system_enviroments_proxy_info_;
}

}  // namespace webos
}  // namespace pal