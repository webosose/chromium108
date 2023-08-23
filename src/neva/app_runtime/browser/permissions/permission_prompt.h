// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/ui/permission_bubble/permission_prompt.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_H_

#include "components/permissions/permission_prompt.h"
#include "components/permissions/request_type.h"

namespace content {
class WebContents;
}

using RequestTypes = std::vector<permissions::RequestType>;

// Factory function to create permission prompts for neva.
std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate);

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_H_
