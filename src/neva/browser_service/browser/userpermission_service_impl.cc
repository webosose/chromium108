// Copyright 2022 LG Electronics, Inc.
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

#include "neva/browser_service/browser/userpermission_service_impl.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace browser {

UserPermissionServiceImpl* UserPermissionServiceImpl::Get() {
  return base::Singleton<UserPermissionServiceImpl>::get();
}

UserPermissionServiceImpl::UserPermissionServiceImpl() {}

void UserPermissionServiceImpl::ShowPrompt(
    const GURL& requesting_url,
    RequestTypes types,
    base::RepeatingCallback<void(Response type)> response_cb) {
  prompt_response_cb_ = response_cb;

  std::vector<int32_t> request_types;
  for (permissions::RequestType type : types)
    request_types.push_back(static_cast<int32_t>(type));

  for (auto& listener : listeners_)
    listener->ShowPrompt(requesting_url.host(), request_types);
}

void UserPermissionServiceImpl::AddBinding(
    mojo::PendingReceiver<mojom::UserPermissionService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UserPermissionServiceImpl::RegisterListener(
    RegisterListenerCallback callback) {
  mojo::AssociatedRemote<mojom::UserPermissionListener> listener;
  std::move(callback).Run(listener.BindNewEndpointAndPassReceiver());
  listeners_.Add(std::move(listener));
}

void UserPermissionServiceImpl::OnPromptResponse(
    int32_t response,
    OnPromptResponseCallback callback) {
  if (!prompt_response_cb_.is_null())
    std::move(prompt_response_cb_).Run(static_cast<Response>(response));

  std::move(callback).Run(true);
}

}  // namespace browser