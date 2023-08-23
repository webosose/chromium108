// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/notification_platform_bridge_delegator.cc.

#include "neva/app_runtime/browser/notifications/notification_platform_bridge_delegator.h"

namespace neva_app_runtime {

NotificationPlatformBridgeDelegator::NotificationPlatformBridgeDelegator(
    content::BrowserContext* context,
    base::OnceClosure ready_callback)
    : context_(context), bridge_(NotificationPlatformBridge::Create()) {
  std::move(ready_callback).Run();
}

NotificationPlatformBridgeDelegator::~NotificationPlatformBridgeDelegator() =
    default;

void NotificationPlatformBridgeDelegator::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  bridge_->Display(notification_type, context_, notification,
                   std::move(metadata));
}

void NotificationPlatformBridgeDelegator::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  bridge_->Close(context_, notification_id);
}

void NotificationPlatformBridgeDelegator::GetDisplayed(
    GetDisplayedNotificationsCallback callback) const {
  bridge_->GetDisplayed(context_, std::move(callback));
}

void NotificationPlatformBridgeDelegator::DisplayServiceShutDown() {
  bridge_->DisplayServiceShutDown(context_);
}

}  // namespace neva_app_runtime
