// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_platform_bridge.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "neva/app_runtime/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "neva/app_runtime/browser/notifications/notification_handler.h"

namespace message_center {
class Notification;
}

namespace neva_app_runtime {

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
  using NotificationBridgeReadyCallback =
      base::OnceCallback<void(bool /* success */)>;

  static std::unique_ptr<NotificationPlatformBridge> Create();

  // Returns whether a native bridge can handle a notification of the given
  // type. Ideally, this would always return true, but for now some platforms
  // can't handle TRANSIENT notifications.
  static bool CanHandleType(NotificationHandler::Type notification_type);

  // Returns a unique string identifier for |profile|.
  static std::string GetProfileId(content::BrowserContext* profile);

  NotificationPlatformBridge(const NotificationPlatformBridge&) = delete;
  NotificationPlatformBridge& operator=(const NotificationPlatformBridge&) =
      delete;
  virtual ~NotificationPlatformBridge() = default;

  // Shows a toast on screen using the data passed in |notification|.
  virtual void Display(
      NotificationHandler::Type notification_type,
      content::BrowserContext* profile,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) = 0;

  // Closes a nofication with |notification_id| and |profile| if being
  // displayed.
  virtual void Close(content::BrowserContext* profile,
                     const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and posts
  // |callback| with the result.
  virtual void GetDisplayed(
      content::BrowserContext* profile,
      GetDisplayedNotificationsCallback callback) const = 0;

  // Calls |callback| once |this| is initialized. The argument is
  // true if |this| is ready to be used and false if initialization
  // failed. |callback| may be called directly or from a posted task.
  virtual void SetReadyCallback(NotificationBridgeReadyCallback callback) = 0;

  // Called when display service for |profile| is being shut down (for example
  // if the profile is being destroyed). If |profile| is nullptr the system
  // notification display service is being shutdown.
  virtual void DisplayServiceShutDown(content::BrowserContext* profile) = 0;

 protected:
  NotificationPlatformBridge() = default;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
