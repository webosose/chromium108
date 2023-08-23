// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/platform_notification_service_factory.cc.

#include "neva/app_runtime/browser/notifications/platform_notification_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "neva/app_runtime/browser/notifications/notification_display_service_factory.h"
#include "neva/app_runtime/browser/notifications/platform_notification_service_impl.h"

namespace neva_app_runtime {

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<PlatformNotificationServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create= */ true));
}

// static
PlatformNotificationServiceFactory*
PlatformNotificationServiceFactory::GetInstance() {
  return base::Singleton<PlatformNotificationServiceFactory>::get();
}

PlatformNotificationServiceFactory::PlatformNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformNotificationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

KeyedService* PlatformNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PlatformNotificationServiceImpl(context);
}

}  // namespace neva_app_runtime
