// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/browser/geolocation/geolocation_permission_context.h"

#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

namespace neva_app_runtime {

GeolocationPermissionContext::GeolocationPermissionContext(
    content::BrowserContext* browser_context)
    : permissions::GeolocationPermissionContext(browser_context, nullptr) {}

GeolocationPermissionContext::~GeolocationPermissionContext() = default;

void GeolocationPermissionContext::DecidePermission(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PermissionContextBase::DecidePermission(id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          std::move(callback));
}

ContentSetting GeolocationPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ContentSetting content_setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

  if (content_setting == CONTENT_SETTING_DEFAULT)
    content_setting = CONTENT_SETTING_ASK;

  return content_setting;
}

}  // namespace neva_app_runtime
