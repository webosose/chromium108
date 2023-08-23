// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/media/webrtc/media_stream_device_permission_context.h

#ifndef NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
#define NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"

class GURL;

namespace permissions {
class PermissionRequestID;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace neva_app_runtime {

using BrowserPermissionCallback = base::OnceCallback<void(ContentSetting)>;

class MediaStreamDevicePermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit MediaStreamDevicePermissionContext(
      content::BrowserContext* browser_context,
      ContentSettingsType content_settings_type);
  ~MediaStreamDevicePermissionContext() override;

  // PermissionContextBase:
  void DecidePermission(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;

  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void ResetPermission(const GURL& requesting_origin,
                       const GURL& embedding_origin) override;

 private:
  bool IsRestrictedToSecureOrigins() const override;

  ContentSettingsType content_settings_type_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICE_PERMISSION_CONTEXT_H_
