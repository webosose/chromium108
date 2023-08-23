// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/notification_display_service_factory.cc.

#include "neva/app_runtime/browser/notifications/notification_display_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "neva/app_runtime/browser/notifications/notification_display_service_impl.h"

namespace neva_app_runtime {

// static
NotificationDisplayService* NotificationDisplayServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<NotificationDisplayService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
NotificationDisplayServiceFactory*
NotificationDisplayServiceFactory::GetInstance() {
  return base::Singleton<NotificationDisplayServiceFactory>::get();
}

NotificationDisplayServiceFactory::NotificationDisplayServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NotificationDisplayService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* NotificationDisplayServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(peter): Register the notification handlers here.
  return new NotificationDisplayServiceImpl(context);
}

}  // namespace neva_app_runtime
