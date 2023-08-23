// Copyright 2015-2019 LG Electronics, Inc.
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

#include "neva/app_runtime/webapp_injection_manager.h"

#include "content/public/browser/render_frame_host.h"
#include "neva/app_runtime/public/mojom/app_runtime_webview.mojom.h"
#include "neva/injection/public/common/webapi_names.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace neva_app_runtime {
namespace {

std::set<std::string> allowed_injections = {
#if defined(ENABLE_BROWSER_SHELL)
    std::string(injections::webapi::kBrowserShell),
    std::string(injections::webapi::kBrowserShellIpc),
#endif
#if defined(ENABLE_WEBOS_SYSTEM_WEBAPI)
    std::string(injections::webapi::kWebOSSystem),
    std::string(injections::webapi::kWebOSSystemObsolete),
#endif
#if defined(ENABLE_WEBOS_SERVICE_BRIDGE_WEBAPI)
    std::string(injections::webapi::kWebOSServiceBridge),
    std::string(injections::webapi::kWebOSServiceBridgeObsolete),
#endif
#if defined(USE_GAV)
    std::string(injections::webapi::kWebOSGAV),
#endif
#if defined(ENABLE_SAMPLE_WEBAPI)
    std::string(injections::webapi::kSample),
#endif
#if defined(ENABLE_BROWSER_CONTROL_WEBAPI)
    std::string(injections::webapi::kBrowserControl),
#endif
#if defined(ENABLE_MEMORYMANAGER_WEBAPI)
    std::string(injections::webapi::kMemoryManager),
#endif
#if defined(ENABLE_NETWORK_ERROR_PAGE_CONTROLLER_WEBAPI)
    std::string(injections::webapi::kNetworkErrorPage),
#endif
#if defined(USE_NEVA_CHROME_EXTENSIONS)
    std::string(injections::webapi::kChromeExtensions),
#endif
};

}  // namespace

WebAppInjectionManager::WebAppInjectionManager() {}

WebAppInjectionManager::~WebAppInjectionManager() {}

void WebAppInjectionManager::RequestLoadInjection(
    content::RenderFrameHost* render_frame_host,
    std::string injection_name,
    std::string arguments_json) {
  if (!render_frame_host)
    return;

  if (allowed_injections.count(injection_name) == 0)
    return;

  if (requested_injections_.find(injection_name) != requested_injections_.end())
    return;

  mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  requested_injections_.insert(std::make_pair(injection_name, arguments_json));
  client->AddInjectionToLoad(std::move(injection_name),
                             std::move(arguments_json));
}

void WebAppInjectionManager::RequestReloadInjections(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  for (const auto& injection : requested_injections_)
    client->AddInjectionToLoad(injection.first, injection.second);
}

void WebAppInjectionManager::RequestUnloadInjections(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  requested_injections_.clear();
  client->UnloadInjections();
}

}  // namespace neva_app_runtime
