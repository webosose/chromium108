// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/platform_notification_service_impl.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/platform_notification_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace neva_app_runtime {

class PlatformNotificationServiceImpl
    : public content::PlatformNotificationService,
      public KeyedService {
 public:
  explicit PlatformNotificationServiceImpl(content::BrowserContext* context);
  PlatformNotificationServiceImpl(const PlatformNotificationServiceImpl&) =
      delete;
  PlatformNotificationServiceImpl& operator=(
      const PlatformNotificationServiceImpl&) = delete;
  ~PlatformNotificationServiceImpl() override;

  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::PlatformNotificationService implementation.
  void DisplayNotification(
      const std::string& notification_id,
      const GURL& origin,
      const GURL& document_url,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources,
      const int64_t service_worker_registration_id) override;
  void CloseNotification(const std::string& notification_id) override;
  void ClosePersistentNotification(const std::string& notification_id) override;
  void GetDisplayedNotifications(
      DisplayedNotificationsCallback callback) override;
  void ScheduleTrigger(base::Time timestamp) override;
  base::Time ReadNextTriggerTimestamp() override;
  int64_t ReadNextPersistentNotificationId() override;
  void RecordNotificationUkmEvent(
      const content::NotificationDatabaseData& data) override;

 private:
  content::BrowserContext* context_ = nullptr;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
