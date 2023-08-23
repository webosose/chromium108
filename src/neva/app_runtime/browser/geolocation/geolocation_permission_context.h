// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/permission_context_base.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"

class PrefService;
class GURL;

namespace permissions {
class PermissionRequestID;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace neva_app_runtime {

class GeolocationPermissionContext
    : public permissions::GeolocationPermissionContext {
 public:
  GeolocationPermissionContext(content::BrowserContext* browser_context);
  GeolocationPermissionContext(const GeolocationPermissionContext&) = delete;
  GeolocationPermissionContext& operator=(const GeolocationPermissionContext&) =
      delete;
  ~GeolocationPermissionContext() override;

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
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
