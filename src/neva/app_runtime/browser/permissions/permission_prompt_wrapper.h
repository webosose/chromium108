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

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WRAPPER_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WRAPPER_H_

#include <memory>

#include "components/permissions/permission_prompt.h"
#include "neva/app_runtime/public/permission_prompt.h"

namespace content {
class WebContents;
}

namespace neva_app_runtime {

class PermissionPromptWrapper
    : public permissions::PermissionPrompt,
      public neva_app_runtime::PermissionPrompt::Delegate {
 public:
  PermissionPromptWrapper(content::WebContents* web_contents,
                          permissions::PermissionPrompt::Delegate* delegate);
  ~PermissionPromptWrapper() override;

  void Init(std::unique_ptr<neva_app_runtime::PermissionPrompt> prompt);

  // permissions::PermissionPrompt implementation
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  // neva_app_runtime::PermissionPrompt::Delegate implementation
  const std::vector<neva_app_runtime::PermissionRequest*>& Requests() override;
  const std::string& GetAppId() const override;
  void Accept() override;
  void AcceptThisTime() override;
  void Deny() override;
  void Closing() override;

 private:
  std::unique_ptr<neva_app_runtime::PermissionPrompt> prompt_;
  permissions::PermissionPrompt::Delegate* delegate_ = nullptr;
  std::string app_id_;
  std::vector<neva_app_runtime::PermissionRequest*> wrapped_requests_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WRAPPER_H_
