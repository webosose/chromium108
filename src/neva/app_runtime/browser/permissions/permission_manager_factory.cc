// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/permissions/permission_manager_factory.cc

#include "neva/app_runtime/browser/permissions/permission_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/permission_util.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/geolocation/geolocation_permission_context.h"
#include "neva/app_runtime/browser/media/webrtc/media_stream_device_permission_context.h"
#include "neva/app_runtime/browser/notifications/notification_permission_context.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace {

// Dummy CameraPanTiltZoomPermissionContext::Delegate
class CameraPanTiltZoomPermissionBypassDelegate
    : public permissions::CameraPanTiltZoomPermissionContext::Delegate {
 public:
  CameraPanTiltZoomPermissionBypassDelegate() = default;
  ~CameraPanTiltZoomPermissionBypassDelegate() override = default;

  bool GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      ContentSetting* content_setting_result) override {
    // return false to call base context's GetPermissionStatusInternal()
    return false;
  }
};

// Permission context which denies all requests.
class DeniedPermissionContext : public permissions::PermissionContextBase {
 public:
  using PermissionContextBase::PermissionContextBase;

  DeniedPermissionContext(const DeniedPermissionContext&) = delete;
  DeniedPermissionContext& operator=(const DeniedPermissionContext&) = delete;

 protected:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_BLOCK;
  }

  bool IsRestrictedToSecureOrigins() const override { return true; }
};

webrtc::MediaStreamDeviceEnumerator* GetMediaStreamDeviceEnumerator() {
  static base::NoDestructor<webrtc::MediaStreamDeviceEnumeratorImpl> instance;
  return instance.get();
}

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    content::BrowserContext* context) {
  // Create default permission contexts initially.
  permissions::PermissionManager::PermissionContextMap permission_contexts;

  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<NotificationPermissionContext>(context);

  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<neva_app_runtime::MediaStreamDevicePermissionContext>(
          context, ContentSettingsType::MEDIASTREAM_MIC);

  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<neva_app_runtime::MediaStreamDevicePermissionContext>(
          context, ContentSettingsType::MEDIASTREAM_CAMERA);

  permission_contexts[ContentSettingsType::CAMERA_PAN_TILT_ZOOM] =
      std::make_unique<permissions::CameraPanTiltZoomPermissionContext>(
          context,
          std::make_unique<CameraPanTiltZoomPermissionBypassDelegate>(),
          GetMediaStreamDeviceEnumerator());

  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<neva_app_runtime::GeolocationPermissionContext>(context);

  // For now, all requests are denied. As features are added, their permission
  // contexts can be added here instead of DeniedPermissionContext.
  for (blink::PermissionType type : blink::GetAllPermissionTypes()) {
    ContentSettingsType content_settings_type =
        permissions::PermissionUtil::PermissionTypeToContentSettingType(type);
    if (permission_contexts.find(content_settings_type) ==
        permission_contexts.end()) {
      permission_contexts[content_settings_type] =
          std::make_unique<DeniedPermissionContext>(
              context, content_settings_type,
              blink::mojom::PermissionsPolicyFeature::kNotFound);
    }
  }
  return permission_contexts;
}
}  // namespace

// static
permissions::PermissionManager* PermissionManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<permissions::PermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PermissionManagerFactory* PermissionManagerFactory::GetInstance() {
  return base::Singleton<PermissionManagerFactory>::get();
}
PermissionManagerFactory::PermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {}
PermissionManagerFactory::~PermissionManagerFactory() {}

// BrowserContextKeyedServiceFactory methods:
KeyedService* PermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::PermissionManager(context,
                                            CreatePermissionContexts(context));
}
