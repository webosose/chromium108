// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/notification_display_service_factory.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace neva_app_runtime {

class NotificationDisplayService;

class NotificationDisplayServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  NotificationDisplayServiceFactory(const NotificationDisplayServiceFactory&) =
      delete;
  NotificationDisplayServiceFactory& operator=(
      const NotificationDisplayServiceFactory&) = delete;
  static NotificationDisplayService* GetForProfile(
      content::BrowserContext* profile);
  static NotificationDisplayServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NotificationDisplayServiceFactory>;

  NotificationDisplayServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
