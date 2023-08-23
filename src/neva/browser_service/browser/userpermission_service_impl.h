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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_USERPERMISSION_SERVICE_IMPL_H_
#define NEVA_BROWSER_SERVICE_BROWSER_USERPERMISSION_SERVICE_IMPL_H_

#include "base/memory/singleton.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "neva/app_runtime/browser/permissions/permission_prompt.h"
#include "neva/browser_service/public/mojom/userpermission_service.mojom.h"
#include "url/gurl.h"

namespace browser {

// This class provides an utility to manage the User permission service
class UserPermissionServiceImpl : public mojom::UserPermissionService {
 public:
  enum Response {
    kError = 0,
    kAcceptPermission,
    kDenyPermission,
    kClosingPermission,
  };

  static UserPermissionServiceImpl* Get();

  void ShowPrompt(const GURL& requesting_url,
                  RequestTypes types,
                  base::RepeatingCallback<void(Response type)> response_cb);

  void AddBinding(mojo::PendingReceiver<mojom::UserPermissionService> receiver);

  // mojom::UserPermissionService
  void RegisterListener(RegisterListenerCallback callback) override;
  void OnPromptResponse(int32_t response,
                        OnPromptResponseCallback callback) override;

 private:
  friend struct base::DefaultSingletonTraits<UserPermissionServiceImpl>;

  UserPermissionServiceImpl();
  UserPermissionServiceImpl(const UserPermissionServiceImpl&) = delete;
  UserPermissionServiceImpl& operator=(const UserPermissionServiceImpl&) =
      delete;
  ~UserPermissionServiceImpl() = default;

  base::RepeatingCallback<void(Response type)> prompt_response_cb_;

  mojo::AssociatedRemoteSet<mojom::UserPermissionListener> listeners_;
  mojo::ReceiverSet<mojom::UserPermissionService> receivers_;
};

}  // namespace browser

#endif  // NEVA_BROWSER_SERVICE_BROWSER_USERPERMISSION_SERVICE_IMPL_H_