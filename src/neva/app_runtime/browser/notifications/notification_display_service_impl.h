// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_display_service_impl.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_

#include "base/containers/queue.h"
#include "neva/app_runtime/browser/notifications/notification_display_service.h"
#include "neva/app_runtime/browser/notifications/notification_platform_bridge_delegator.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

// Implementation of the NotificationDisplayService interface. Methods that are
// not available in the base interface should only be used by the platform
// notification bridges.
class NotificationDisplayServiceImpl : public NotificationDisplayService {
 public:
  // Note that |profile| might be nullptr for notification display service used
  // for system notifications. The system instance is owned by
  // SystemNotificationHelper, and is only expected to handle TRANSIENT
  // notifications.
  explicit NotificationDisplayServiceImpl(content::BrowserContext* profile);
  NotificationDisplayServiceImpl(const NotificationDisplayServiceImpl&) =
      delete;
  NotificationDisplayServiceImpl& operator=(
      const NotificationDisplayServiceImpl&) = delete;
  ~NotificationDisplayServiceImpl() override;

  // KeyedService implementation:
  void Shutdown() override;

  // NotificationDisplayService implementation:
  void Display(NotificationHandler::Type notification_type,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationHandler::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(DisplayedNotificationsCallback callback) override;

 private:
  // Called when the NotificationPlatformBridgeDelegator has been initialized.
  void OnNotificationPlatformBridgeReady();

  content::BrowserContext* context_;

  // This NotificationPlatformBridgeDelegator delegates to either the native
  // bridge or to the MessageCenter if there is no native bridge or it does not
  // support certain notification types.
  std::unique_ptr<NotificationPlatformBridgeDelegator> bridge_delegator_;

  // Tasks that need to be run once the display bridge has been initialized.
  base::queue<base::OnceClosure> actions_;

  // Boolean tracking whether the |bridge_delegator_| has been initialized.
  bool bridge_delegator_initialized_ = false;

  base::WeakPtrFactory<NotificationDisplayServiceImpl> weak_factory_{this};
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_IMPL_H_
