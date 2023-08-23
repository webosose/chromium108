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
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

// static
void NotificationPermissionContext::UpdatePermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSetting setting) {}

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

void NotificationPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  DCHECK_EQ(requesting_origin, requesting_origin.DeprecatedGetOriginAsURL());
  DCHECK_EQ(embedding_origin, embedding_origin.DeprecatedGetOriginAsURL());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  // non-local file format (e.g. file://com.webos.app.test)
  if (requesting_origin.SchemeIsFile() && !requesting_origin.host().empty()) {
    content_settings::PatternPair patterns =
        GetContentSettingPatternsForNonLocalType(requesting_origin,
                                                 embedding_origin);

    ContentSettingsPattern primary_pattern = patterns.first;
    ContentSettingsPattern secondary_pattern = patterns.second;

    if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
      return;

    using Constraints = content_settings::ContentSettingConstraints;
    permissions::PermissionsClient::Get()
        ->GetSettingsMap(browser_context())
        ->SetContentSettingCustomScope(
            primary_pattern, secondary_pattern, content_settings_type(),
            content_setting,
            is_one_time ? Constraints{base::Time(),
                                      content_settings::SessionModel::OneTime}
                        : Constraints());
    return;
  }

  permissions::PermissionContextBase::UpdateContentSetting(
      requesting_origin, embedding_origin, content_setting, is_one_time);
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}

content_settings::PatternPair
NotificationPermissionContext::GetContentSettingPatternsForNonLocalType(
    const GURL& primary_url,
    const GURL& secondary_url) {
  DCHECK(!primary_url.is_empty());
  content_settings::PatternPair patterns;

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  builder->WithScheme(primary_url.scheme())->WithHost(primary_url.host());

  patterns.first = builder->Build();
  patterns.second = ContentSettingsPattern::Wildcard();
  return patterns;
}
