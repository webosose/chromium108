// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WEBOS_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WEBOS_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "neva/app_runtime/browser/permissions/permission_prompt.h"

namespace content {
class WebContents;
}  // namespace content

// This object will create or trigger system UI to reflect that a website is
// requesting a permission.
class PermissionPromptWebOS : public permissions::PermissionPrompt {
 public:
  class PlatformDelegate {
   public:
    PlatformDelegate(permissions::PermissionPrompt::Delegate* delegate);
    virtual ~PlatformDelegate() {}
    virtual void ShowBubble(const GURL& origin_url, RequestTypes types) = 0;

   protected:
    const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;
  };

  PermissionPromptWebOS(content::WebContents* web_contents,
                        permissions::PermissionPrompt::Delegate* delegate,
                        std::unique_ptr<PlatformDelegate> platform_delegate);

  PermissionPromptWebOS(const PermissionPromptWebOS&) = delete;
  PermissionPromptWebOS& operator=(const PermissionPromptWebOS&) = delete;

  ~PermissionPromptWebOS() override;

  // permissions::PermissionPrompt:
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
  bool UpdateAnchor() override { return false; }
  TabSwitchingBehavior GetTabSwitchingBehavior() override;

 private:
  void ShowBubble();

  void AcceptPermission();
  void DenyPermission();
  void ClosingPermission();

  bool ShouldShowRequest(permissions::RequestType type) const;
  std::vector<permissions::PermissionRequest*> GetVisibleRequests() const;
  RequestTypes GetPermissionRequestTypes() const;

  std::string app_id_{};
  content::WebContents* web_contents_;
  const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  std::unique_ptr<PlatformDelegate> platform_delegate_;
};

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_PROMPT_WEBOS_H_
