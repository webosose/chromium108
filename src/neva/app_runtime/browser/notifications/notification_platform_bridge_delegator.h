// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/notification_platform_bridge_delegator.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "neva/app_runtime/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "neva/app_runtime/browser/notifications/notification_handler.h"
#include "neva/app_runtime/browser/notifications/notification_platform_bridge.h"
#include "ui/message_center/public/cpp/notification.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

// This class is responsible for delegating notification events to either the
// system NotificationPlatformBridge or fall back to Chrome's own message
// center implementation. This can happen if there is no system support for
// notifications on this platform (or it has been disabled via flags). We also
// delegate to the message center if the given notification type is not
// supported on the system bridge.
class NotificationPlatformBridgeDelegator {
 public:
  NotificationPlatformBridgeDelegator(content::BrowserContext* context,
                                      base::OnceClosure ready_callback);
  NotificationPlatformBridgeDelegator(
      const NotificationPlatformBridgeDelegator&) = delete;
  NotificationPlatformBridgeDelegator& operator=(
      const NotificationPlatformBridgeDelegator&) = delete;
  virtual ~NotificationPlatformBridgeDelegator();

  virtual void Display(NotificationHandler::Type notification_type,
                       const message_center::Notification& notification,
                       std::unique_ptr<NotificationCommon::Metadata> metadata);

  virtual void Close(NotificationHandler::Type notification_type,
                     const std::string& notification_id);

  virtual void GetDisplayed(GetDisplayedNotificationsCallback callback) const;

  virtual void DisplayServiceShutDown();

 private:
  content::BrowserContext* context_;

  std::unique_ptr<NotificationPlatformBridge> bridge_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_
