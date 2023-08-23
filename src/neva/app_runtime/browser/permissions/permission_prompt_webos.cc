// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/ui/views/permission_bubble/permission_prompt_impl.cc

#include "neva/app_runtime/browser/permissions/permission_prompt_webos.h"

#include <iterator>
#include <memory>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/permissions/permission_uma_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

std::unique_ptr<permissions::PermissionPrompt> CreatePermissionPrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  return std::make_unique<PermissionPromptWebOS>(web_contents, delegate,
                                                 nullptr);
}

PermissionPromptWebOS::PlatformDelegate::PlatformDelegate(
    permissions::PermissionPrompt::Delegate* delegate)
    : delegate_(delegate) {}

PermissionPromptWebOS::PermissionPromptWebOS(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate,
    std::unique_ptr<PlatformDelegate> platform_delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      platform_delegate_(std::move(platform_delegate)) {
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents);
  std::string application_id =
      web_contents_impl->GetRendererPrefs().application_id;
  if (!application_id.empty())
    app_id_ = std::move(application_id);

  ShowBubble();
}

PermissionPromptWebOS::~PermissionPromptWebOS() {}

permissions::PermissionPromptDisposition
PermissionPromptWebOS::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::NOT_APPLICABLE;
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptWebOS::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

void PermissionPromptWebOS::ShowBubble() {
  RequestTypes types = GetPermissionRequestTypes();
  GURL requesting_origin = delegate_->GetRequestingOrigin();
  if (platform_delegate_) {
    platform_delegate_->ShowBubble(requesting_origin, types);
    return;
  }

  bool permission_granted = false;
  content::WebContentsDelegate* web_contents_delegate =
      web_contents_->GetDelegate();
  for (permissions::RequestType type : types) {
    switch (type) {
      case permissions::RequestType::kCameraPanTiltZoom:
      case permissions::RequestType::kCameraStream:
        if (web_contents_delegate)
          permission_granted = web_contents_delegate->VideoCaptureAllowed();
        break;
      case permissions::RequestType::kMicStream:
        if (web_contents_->GetDelegate())
          permission_granted = web_contents_delegate->AudioCaptureAllowed();
        break;
      default:
        permission_granted = true;
        break;
    }
  }

  VLOG(1) << __func__ << " origin_url=" << requesting_origin
          << " permission_granted=" << permission_granted;
  if (permission_granted)
    AcceptPermission();
  else
    ClosingPermission();
}

void PermissionPromptWebOS::AcceptPermission() {
  delegate_->Accept();
}

void PermissionPromptWebOS::DenyPermission() {
  delegate_->Deny();
}

void PermissionPromptWebOS::ClosingPermission() {
  delegate_->Dismiss();
}

bool PermissionPromptWebOS::ShouldShowRequest(
    permissions::RequestType type) const {
  if (type == permissions::RequestType::kCameraStream) {
    // Hide camera request if camera PTZ request is present as well.
    auto requests = delegate_->Requests();
    return std::find_if(requests.begin(), requests.end(), [](auto* request) {
             return request->request_type() ==
                    permissions::RequestType::kCameraPanTiltZoom;
           }) == requests.end();
  }
  return true;
}

std::vector<permissions::PermissionRequest*>
PermissionPromptWebOS::GetVisibleRequests() const {
  std::vector<permissions::PermissionRequest*> visible_requests;
  for (permissions::PermissionRequest* request : delegate_->Requests()) {
    if (ShouldShowRequest(request->request_type()))
      visible_requests.push_back(request);
  }
  return visible_requests;
}

RequestTypes PermissionPromptWebOS::GetPermissionRequestTypes() const {
  RequestTypes permission_request_types;
  for (permissions::PermissionRequest* request : GetVisibleRequests())
    permission_request_types.push_back(request->request_type());

  return permission_request_types;
}
