// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/notification_platform_bridge_delegator.cc.

#include "neva/app_runtime/browser/notifications/notification_platform_bridge_delegator.h"

#include "base/notreached.h"
#include "neva/app_runtime/browser/notifications/notification_platform_bridge_webos.h"
#include "neva/app_runtime/browser/notifications/notification_wrapper.h"
#include "neva/app_runtime/public/platform_factory.h"

namespace neva_app_runtime {

NotificationPlatformBridgeDelegator::NotificationPlatformBridgeDelegator(
    content::BrowserContext* context,
    base::OnceClosure ready_callback)
    : context_(context) {
  if (GetPlatformFactory()) {
    bridge_ = GetPlatformFactory()->CreateNotificationPlatformBridge();
  }

  if (!bridge_) {
    bridge_.reset(new NotificationPlatformBridgeWebos());
  }
  std::move(ready_callback).Run();
}

NotificationPlatformBridgeDelegator::~NotificationPlatformBridgeDelegator() =
    default;

void NotificationPlatformBridgeDelegator::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata>) {
  NotificationWrapper wrapped(notification);
  bridge_->Display(wrapped);
}

void NotificationPlatformBridgeDelegator::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  bridge_->Close(notification_id);
}

void NotificationPlatformBridgeDelegator::GetDisplayed(
    GetDisplayedNotificationsCallback callback) const {
  bridge_->GetDisplayed(std::move(callback));
}

void NotificationPlatformBridgeDelegator::DisplayServiceShutDown() {
  NOTIMPLEMENTED();
}

}  // namespace neva_app_runtime
