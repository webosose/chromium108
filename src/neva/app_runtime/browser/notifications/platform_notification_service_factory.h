// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/platform_notification_service_factory.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace neva_app_runtime {

class PlatformNotificationServiceImpl;

class PlatformNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PlatformNotificationServiceFactory(
      const PlatformNotificationServiceFactory&) = delete;
  PlatformNotificationServiceFactory& operator=(
      const PlatformNotificationServiceFactory&) = delete;

  static PlatformNotificationServiceImpl* GetForProfile(
      content::BrowserContext* profile);
  static PlatformNotificationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      PlatformNotificationServiceFactory>;

  PlatformNotificationServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_
