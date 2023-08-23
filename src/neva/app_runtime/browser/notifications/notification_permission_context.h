// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/notifications/notification_permission_context.h

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
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

using BrowserPermissionCallback = base::OnceCallback<void(ContentSetting)>;

class NotificationPermissionContext
    : public permissions::PermissionContextBase {
 public:
  // Helper method for updating the permission state of |origin| to |setting|.
  static void UpdatePermission(content::BrowserContext* browser_context,
                               const GURL& origin,
                               ContentSetting setting);

  explicit NotificationPermissionContext(
      content::BrowserContext* browser_context);
  ~NotificationPermissionContext() override;

 private:
  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void DecidePermission(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;
  bool IsRestrictedToSecureOrigins() const override;

  content_settings::PatternPair GetContentSettingPatternsForNonLocalType(
      const GURL& primary_url,
      const GURL& secondary_url);

  base::WeakPtrFactory<NotificationPermissionContext> weak_factory_ui_thread_{
      this};
};

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_H_
