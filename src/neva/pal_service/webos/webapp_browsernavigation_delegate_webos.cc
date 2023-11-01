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

#include "neva/pal_service/webos/webapp_browsernavigation_delegate_webos.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "neva/pal_service/luna/luna_names.h"

namespace pal {
namespace webos {

WebAppBrowserNavigationDelegateWebOS::WebAppBrowserNavigationDelegateWebOS()
    : luna_client_(InitLunaClient()), weak_ptr_factory_(this) {}

// static
std::unique_ptr<luna::Client>
WebAppBrowserNavigationDelegateWebOS::InitLunaClient() {
  luna::Client::Params params;
  params.name =
      luna::GetServiceNameWithRandSuffix(luna::service_name::kChromiumPwa);
  return luna::CreateClient(params);
}

void WebAppBrowserNavigationDelegateWebOS::OpenUrlInBrowser(
    const std::string& url) {
  if (luna_client_ && luna_client_->IsInitialized()) {
    // luna-send -n 1 -f luna://com.webos.applicationManager/launch '{"id":
    // "com.webos.app.enactbrowser",
    // "params":{"target":"url"}}'
    std::string service_uri = pal::luna::GetServiceURI(
        pal::luna::service_uri::kApplicationManager, "launch");
    std::string params =
        "{\"id\": \"com.webos.app.enactbrowser\", \"params\":{\"target\":\"" +
        url + "\"}}";
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << ": "
            << service_uri << " " << params;

    luna_client_->Call(
        std::move(service_uri), std::move(params),
        base::BindOnce(
            &WebAppBrowserNavigationDelegateWebOS::OnOpenUrlInBrowser,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebAppBrowserNavigationDelegateWebOS::OnOpenUrlInBrowser(
    pal::luna::Client::ResponseStatus status,
    unsigned /*token*/,
    const std::string& json) {
  VLOG(1) << __func__ << "() status=" << static_cast<int>(status)
          << ", response='" << json << "'";
}

}  // namespace webos
}  // namespace pal
