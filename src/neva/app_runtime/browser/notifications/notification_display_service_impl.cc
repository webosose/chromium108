// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_display_service_impl.cc.

#include "neva/app_runtime/browser/notifications/notification_display_service_impl.h"

#include "content/public/browser/browser_thread.h"
#include "neva/app_runtime/browser/wrap_callback.h"

namespace neva_app_runtime {

NotificationDisplayServiceImpl::NotificationDisplayServiceImpl(
    content::BrowserContext* context)
    : context_(context) {
  bridge_delegator_ = std::make_unique<NotificationPlatformBridgeDelegator>(
      context_,
      base::BindOnce(
          &NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady,
          weak_factory_.GetWeakPtr()));
}

NotificationDisplayServiceImpl::~NotificationDisplayServiceImpl() = default;

void NotificationDisplayServiceImpl::Shutdown() {
  bridge_delegator_->DisplayServiceShutDown();
}

void NotificationDisplayServiceImpl::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // TODO(estade): in the future, the reverse should also be true: a
  // non-TRANSIENT type implies no delegate.
  if (notification_type == NotificationHandler::Type::TRANSIENT)
    DCHECK(notification.delegate());

  CHECK(context_ || notification_type == NotificationHandler::Type::TRANSIENT);

  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Display,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification, std::move(metadata)));
    return;
  }

  bridge_delegator_->Display(notification_type, notification,
                             std::move(metadata));
}

void NotificationDisplayServiceImpl::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  CHECK(context_ || notification_type == NotificationHandler::Type::TRANSIENT);

  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::Close,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification_id));
    return;
  }

  bridge_delegator_->Close(notification_type, notification_id);
}

void NotificationDisplayServiceImpl::GetDisplayed(
    DisplayedNotificationsCallback callback) {
  if (!bridge_delegator_initialized_) {
    actions_.push(base::BindOnce(&NotificationDisplayServiceImpl::GetDisplayed,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(callback)));
    return;
  }

  bridge_delegator_->GetDisplayed(GetDisplayedNotificationsCallback(
      std::make_unique<WrapOnceCallback<void(std::set<std::string>, bool)>>(
          base::BindOnce(&NotificationDisplayServiceImpl::OnGetDisplayed,
                         weak_factory_.GetWeakPtr(), std::move(callback)))));
}

void NotificationDisplayServiceImpl::OnNotificationPlatformBridgeReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_delegator_initialized_ = true;

  // Flush any pending actions that have yet to execute.
  while (!actions_.empty()) {
    std::move(actions_.front()).Run();
    actions_.pop();
  }
}

void NotificationDisplayServiceImpl::OnGetDisplayed(
    DisplayedNotificationsCallback callback,
    std::set<std::string> notification_ids,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(std::move(notification_ids),
                          supports_synchronization);
}

}  // namespace neva_app_runtime
