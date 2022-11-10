// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/notifications/notification_permission_context.cc

#include "neva/app_runtime/browser/notifications/notification_permission_context.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "neva/app_runtime/app/app_runtime_main_delegate.h"
#include "neva/app_runtime/browser/host_content_settings_map_factory.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

// static
void NotificationPermissionContext::UpdatePermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSetting setting) {
  ContentSettingsType type = ContentSettingsType::NOTIFICATIONS;
  const GURL primary_url =
      PermissionContextBase::convertToApplicationURL(origin);
  const GURL secondary_url = GURL();
  HostContentSettingsMap* host_content_settings_map =
      neva_app_runtime::HostContentSettingsMapFactory::GetForBrowserContext(
          browser_context);
  ContentSetting generated_setting =
      host_content_settings_map->GetContentSetting(primary_url, secondary_url,
                                                   type);

  if (generated_setting == setting)
    return;

  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_BLOCK:
    case CONTENT_SETTING_DEFAULT:
      host_content_settings_map->SetContentSettingDefaultScope(
          primary_url, secondary_url, type, setting);
      break;

    default:
      NOTREACHED();
  }
}

NotificationPermissionContext::NotificationPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::NOTIFICATIONS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

NotificationPermissionContext::~NotificationPermissionContext() {}

ContentSetting NotificationPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  ContentSetting content_setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

  if (requesting_origin != embedding_origin &&
      content_setting == CONTENT_SETTING_ASK)
    return CONTENT_SETTING_BLOCK;

  return content_setting;
}

void NotificationPermissionContext::DecidePermission(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // PermissionRequestManager::CreateForWebContents must have been called
  // during web contents  initialization.
  permissions::PermissionContextBase::DecidePermission(
      id, requesting_origin, embedding_origin, user_gesture,
      std::move(callback));
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
