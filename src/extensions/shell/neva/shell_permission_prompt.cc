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

#include "extensions/shell/neva/shell_permission_prompt.h"

#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/permissions/permission_uma_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

ShellPermissionPrompt::ShellPermissionPrompt(
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptWebOS::PlatformDelegate(delegate) {}

ShellPermissionPrompt::~ShellPermissionPrompt() = default;

void ShellPermissionPrompt::ShowBubble(const GURL& origin_url,
                                       RequestTypes types) {
  browser::UserPermissionServiceImpl::Get()->ShowPrompt(
      origin_url, types,
      base::BindPostTask(
          base::SequencedTaskRunnerHandle::Get(),
          base::BindRepeating(&ShellPermissionPrompt::OnPromptResponse,
                              base::Unretained(this))));
}

void ShellPermissionPrompt::OnPromptResponse(
    browser::UserPermissionServiceImpl::Response type) {
  VLOG(1) << __func__ << " type=" << type;

  switch (type) {
    case browser::UserPermissionServiceImpl::kAcceptPermission:
      AcceptPermission();
      break;
    case browser::UserPermissionServiceImpl::kDenyPermission:
      DenyPermission();
      break;
    case browser::UserPermissionServiceImpl::kClosingPermission:
      ClosingPermission();
      break;
    default:
      break;
  }
}

void ShellPermissionPrompt::AcceptPermission() {
  delegate_->Accept();
}

void ShellPermissionPrompt::DenyPermission() {
  delegate_->Deny();
}

void ShellPermissionPrompt::ClosingPermission() {
  delegate_->Dismiss();
}

NevaPermissionsClientDelegate::NevaPermissionsClientDelegate() = default;

NevaPermissionsClientDelegate::~NevaPermissionsClientDelegate() = default;

std::unique_ptr<permissions::PermissionPrompt>
NevaPermissionsClientDelegate::CreatePrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  return std::make_unique<PermissionPromptWebOS>(
      web_contents, delegate,
      std::make_unique<ShellPermissionPrompt>(delegate));
}
