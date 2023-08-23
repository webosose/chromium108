// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_common.cc.

#include "neva/app_runtime/browser/notifications/notification_common.h"

namespace neva_app_runtime {

NotificationCommon::Metadata::~Metadata() = default;

PersistentNotificationMetadata::PersistentNotificationMetadata() {
  type = NotificationHandler::Type::WEB_PERSISTENT;
}

PersistentNotificationMetadata::~PersistentNotificationMetadata() = default;

// static
const PersistentNotificationMetadata* PersistentNotificationMetadata::From(
    const Metadata* metadata) {
  if (!metadata || metadata->type != NotificationHandler::Type::WEB_PERSISTENT)
    return nullptr;

  return static_cast<const PersistentNotificationMetadata*>(metadata);
}

NonPersistentNotificationMetadata::NonPersistentNotificationMetadata() {
  type = NotificationHandler::Type::WEB_NON_PERSISTENT;
}

NonPersistentNotificationMetadata::~NonPersistentNotificationMetadata() =
    default;

// static
const NonPersistentNotificationMetadata*
NonPersistentNotificationMetadata::From(const Metadata* metadata) {
  if (!metadata ||
      metadata->type != NotificationHandler::Type::WEB_NON_PERSISTENT)
    return nullptr;

  return static_cast<const NonPersistentNotificationMetadata*>(metadata);
}

}  // namespace neva_app_runtime
