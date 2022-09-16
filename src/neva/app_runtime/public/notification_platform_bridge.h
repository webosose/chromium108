// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_platform_bridge.h.

#ifndef NEVA_APP_RUNTIME_PUBLIC_NOTIFICATION_PLATFORM_BRIDGE_H_
#define NEVA_APP_RUNTIME_PUBLIC_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "neva/app_runtime/public/callback_helper.h"
#include "neva/app_runtime/public/displayed_notifications_dispatch_callback.h"
#include "neva/app_runtime/public/notification.h"

namespace neva_app_runtime {

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
  using NotificationBridgeReadyCallback =
      CallbackHelper<void(bool /* success */)>;

  virtual ~NotificationPlatformBridge() {}

  // Shows a toast on screen using the data passed in |notification|.
  virtual void Display(const Notification& notification) = 0;

  // Closes a nofication with |notification_id| and |profile| if being
  // displayed.
  virtual void Close(const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and posts
  // |callback| with the result.
  virtual void GetDisplayed(
      GetDisplayedNotificationsCallback callback) const = 0;

  // Calls |callback| once |this| is initialized. The argument is
  // true if |this| is ready to be used and false if initialization
  // failed. |callback| may be called directly or from a posted task.
  virtual void SetReadyCallback(NotificationBridgeReadyCallback callback) = 0;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_PUBLIC_NOTIFICATION_PLATFORM_BRIDGE_H_
