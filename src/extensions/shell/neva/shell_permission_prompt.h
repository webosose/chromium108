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

#ifndef EXTENSIONS_SHELL_NEVA_SHELL_PERMISSION_PROMPT_H_
#define EXTENSIONS_SHELL_NEVA_SHELL_PERMISSION_PROMPT_H_

#include "neva/app_runtime/browser/permissions/neva_permissions_client.h"
#include "neva/app_runtime/browser/permissions/permission_prompt_webos.h"
#include "neva/browser_service/browser/userpermission_service_impl.h"

// This object will create or trigger system UI to reflect that a website is
// requesting a permission.
class ShellPermissionPrompt : public PermissionPromptWebOS::PlatformDelegate {
 public:
  ShellPermissionPrompt(permissions::PermissionPrompt::Delegate* delegate);

  ShellPermissionPrompt(const ShellPermissionPrompt&) = delete;
  ShellPermissionPrompt& operator=(const ShellPermissionPrompt&) = delete;
  ~ShellPermissionPrompt() override;

  // PermissionPromptWebOS::Delegate
  void ShowBubble(const GURL& origin_url, RequestTypes types) override;

 private:
  void OnPromptResponse(browser::UserPermissionServiceImpl::Response type);

  void AcceptPermission();
  void DenyPermission();
  void ClosingPermission();
};

class NevaPermissionsClientDelegate
    : public neva_app_runtime::NevaPermissionsClient::Delegate {
 public:
  NevaPermissionsClientDelegate();
  NevaPermissionsClientDelegate(const NevaPermissionsClientDelegate&) = delete;
  NevaPermissionsClientDelegate& operator=(
      const NevaPermissionsClientDelegate&) = delete;
  ~NevaPermissionsClientDelegate() override;

  std::unique_ptr<permissions::PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate* delegate) override;
};

#endif  // EXTENSIONS_SHELL_NEVA_SHELL_PERMISSION_PROMPT_H_
