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

#include "neva/app_runtime/browser/permissions/permission_prompt_wrapper.h"

#include "components/permissions/permission_uma_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "neva/app_runtime/browser/permissions/permission_request_impl.h"

namespace neva_app_runtime {
PermissionPromptWrapper::PermissionPromptWrapper(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : delegate_(delegate) {
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents);
  std::string application_id =
      web_contents_impl->GetRendererPrefs().application_id;
  if (!application_id.empty())
    app_id_ = std::move(application_id);

  // To use similar return type for Requests() with
  // permissions::PermissionPrompt::Delegate::Requests() raw pointer is used to
  // wrap pointer to permissions::PermissionRequest
  // the raw pointer will be deleted on dtor
  for (auto& request : delegate_->Requests()) {
    wrapped_requests_.push_back(
        new neva_app_runtime::PermissionRequestImpl(request));
  }
}

PermissionPromptWrapper::~PermissionPromptWrapper() {
  for (auto& r : wrapped_requests_)
    delete r;
}

void PermissionPromptWrapper::Init(
    std::unique_ptr<neva_app_runtime::PermissionPrompt> prompt) {
  prompt_ = std::move(prompt);
}

bool PermissionPromptWrapper::UpdateAnchor() {
  return false;
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptWrapper::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

permissions::PermissionPromptDisposition
PermissionPromptWrapper::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::NOT_APPLICABLE;
}

const std::vector<neva_app_runtime::PermissionRequest*>&
PermissionPromptWrapper::Requests() {
  return wrapped_requests_;
}

const std::string& PermissionPromptWrapper::GetAppId() const {
  return app_id_;
}

void PermissionPromptWrapper::Accept() {
  delegate_->Accept();
}

void PermissionPromptWrapper::AcceptThisTime() {
  delegate_->AcceptThisTime();
}

void PermissionPromptWrapper::Deny() {
  delegate_->Deny();
}

void PermissionPromptWrapper::Closing() {
  delegate_->Dismiss();
}

}  // namespace neva_app_runtime
