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

#include "neva/injection/renderer/browser_shell/browser_shell_permission_request.h"

namespace injections {

gin::WrapperInfo PermissionRequest::kWrapperInfo = {gin::kEmbedderNativeGin};

PermissionRequest::PermissionRequest(Delegate* delegate, uint64_t id)
    : delegate_(delegate), id_(id) {}

PermissionRequest::~PermissionRequest() = default;

gin::ObjectTemplateBuilder PermissionRequest::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PermissionRequest>::GetObjectTemplateBuilder(isolate)
      .SetMethod("allow", &PermissionRequest::Allow)
      .SetMethod("deny", &PermissionRequest::Deny);
}

void PermissionRequest::Allow() {
  if (!delegate_)
    return;

  delegate_->AckPermission(true, id_);
  delegate_ = nullptr;
}

void PermissionRequest::Deny() {
  if (!delegate_)
    return;

  delegate_->AckPermission(false, id_);
  delegate_ = nullptr;
}

}  // namespace injections
