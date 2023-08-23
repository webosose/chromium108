// Copyright 2021 LG Electronics, Inc.
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

#include "neva/browser_shell/service/browser_shell_service_impl.h"

#include <list>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_page_view.h"
#include "neva/app_runtime/app/app_runtime_shell.h"
#include "neva/app_runtime/app/app_runtime_shell_environment.h"
#include "neva/app_runtime/app/app_runtime_shell_window.h"
#include "neva/browser_shell/service/browser_shell_ipc_endpoint_impl.h"
#include "neva/browser_shell/service/browser_shell_page_contents_impl.h"
#include "neva/browser_shell/service/browser_shell_page_view_impl.h"
#include "neva/browser_shell/service/browser_shell_storage_partition_name.h"
#include "neva/browser_shell/service/browser_shell_webrequest_impl.h"
#include "neva/browser_shell/service/browser_shell_window_impl.h"
#include "neva/injection/public/common/webapi_names.h"

namespace browser_shell {

namespace {

std::list<std::string> GetAPIsFromPageContentParams(
    const base::Value::Dict& dict) {
  std::list<std::string> result;
  const base::Value::List* api = dict.FindList("api");
  if (!api)
    return result;

  std::set<std::string> api_set;
  for (const auto& item : *api) {
    if (item.is_string())
      api_set.insert(item.GetString());
  }
  std::copy(api_set.cbegin(), api_set.cend(), std::back_inserter(result));
  return result;
}

std::string GetStringValueFromDict(const base::Value::Dict& dict,
                                  std::string key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

void SetContentParams(neva_app_runtime::PageContents::CreateParams& params,
                      const base::Value::Dict& dict) {
  params.injections = GetAPIsFromPageContentParams(dict);
  params.user_agent = GetStringValueFromDict(dict, "user-agent");
  ParseStoragePartitionName(GetStringValueFromDict(dict, "partition"),
                            params.storage_partition_name,
                            params.storage_partition_off_the_record);
  params.default_access_to_media = dict.FindBool("media-access");
  params.allow_file_access_from_file_urls =
      dict.FindBool("allow-file-access").value_or(false);
  params.allow_universal_access_from_file_urls =
      dict.FindBool("allow-universal-access").value_or(false);
  params.error_page_hiding = dict.FindBool("error-page-hiding").value_or(false);
}

}  // namespace

ShellServiceImpl::ShellServiceImpl(
    std::unique_ptr<neva_app_runtime::Shell> shell)
    : shell_(std::move(shell)) {}

ShellServiceImpl::~ShellServiceImpl() = default;

void ShellServiceImpl::AddBinding(
    mojo::PendingReceiver<mojom::ShellService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ShellServiceImpl::BindRemoteClient(
    mojo::PendingRemote<mojom::ShellServiceClient> client_remote) {
  auto id = remotes_.Add(std::move(client_remote));
  remotes_.Get(id)->SetLaunchParams(shell_->GetLaunchParams());
}

void ShellServiceImpl::BindShellWindow(
    mojo::PendingReceiver<mojom::ShellWindow> receiver,
    BindShellWindowCallback callback) {
  auto* window = shell_->GetMainWindow();
  // It's only 1 shell window allowed yet.
  if (window && shell_window_receivers_.empty()) {
    const std::string name("shell_main_window");
    auto shell_window_impl = std::make_unique<ShellWindowImpl>(window, name);
    shell_window_receivers_.Add(std::move(shell_window_impl),
                                std::move(receiver));
    std::move(callback).Run(name);
    return;
  }
  std::move(callback).Run(std::string());
}

void ShellServiceImpl::CreatePageView(
    mojo::PendingReceiver<mojom::PageView> receiver,
    const std::string& json,
    CreatePageViewCallback callback) {
  auto page_view = std::make_unique<neva_app_runtime::PageView>();
  auto content_params = shell_->GetDefaultContentsParams();
  absl::optional<base::Value> val = base::JSONReader::Read(json);
  if (val && val->is_dict()) {
    base::Value::Dict* dict = val->GetDict().FindDict("page-contents-params");
    if (dict)
      SetContentParams(content_params, *dict);
  }

  auto page_contents =
      std::make_unique<neva_app_runtime::PageContents>(content_params);
  page_view->SetPageContents(std::move(page_contents));
  auto page_view_impl = std::make_unique<PageViewImpl>(page_view.get());

  neva_app_runtime::ShellEnvironment::GetInstance()->SaveDetachedView(
      std::move(page_view));

  const uint64_t id = page_view_impl->GetID();
  mojo::MakeSelfOwnedReceiver(std::move(page_view_impl), std::move(receiver));
  std::move(callback).Run(id);
}

void ShellServiceImpl::CreatePageContents(
    mojo::PendingReceiver<mojom::PageContents> receiver,
    const std::string& json,
    CreatePageContentsCallback callback) {
  auto content_params = shell_->GetDefaultContentsParams();
  absl::optional<base::Value> val = base::JSONReader::Read(json);
  if (val && val->is_dict())
    SetContentParams(content_params, val->GetDict());

  auto page_contents =
      std::make_unique<neva_app_runtime::PageContents>(content_params);
  auto page_contents_impl =
      std::make_unique<PageContentsImpl>(page_contents.get());

  neva_app_runtime::ShellEnvironment::GetInstance()->SaveDetachedContents(
      std::move(page_contents));

  const uint64_t id = page_contents_impl->GetID();
  auto info = browser_shell::mojom::PageContentsCreationInfo::New(
      page_contents_impl->GetActiveState(),
      page_contents_impl->GetErrorPageHiding(),
      page_contents_impl->GetUserAgent(),
      page_contents_impl->GetZoomFactor());

  mojo::MakeSelfOwnedReceiver(std::move(page_contents_impl),
                              std::move(receiver));
  std::move(callback).Run(id, std::move(info));
}

void ShellServiceImpl::CreateShellIpcEndpoint(
    mojo::PendingReceiver<mojom::ShellIpcEndpoint> receiver,
    const std::string& channel) {
  auto shell_ipc_endpoint_impl = std::make_unique<ShellIpcEndpointImpl>(
      channel, &shell_ipc_);

  mojo::MakeSelfOwnedReceiver(std::move(shell_ipc_endpoint_impl),
                              std::move(receiver));
}

void ShellServiceImpl::CreateWebRequest(
    mojo::PendingReceiver<mojom::WebRequest> receiver,
    const std::string& partition) {
  auto webrequest_impl = std::make_unique<WebRequestImpl>(partition);
  webrequest_receivers_.Add(std::move(webrequest_impl), std::move(receiver));
}

}  // namespace browser_shell
