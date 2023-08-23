// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_common.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_

#include "neva/app_runtime/browser/notifications/notification_handler.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

// Shared functionality for both in page and persistent notification
class NotificationCommon {
 public:
  // Things as user can do to a notification. Keep in sync with the
  // NotificationOperation enumeration in notification_response_builder_mac.h.
  enum Operation {
    OPERATION_CLICK = 0,
    OPERATION_CLOSE = 1,
    OPERATION_DISABLE_PERMISSION = 2,
    OPERATION_SETTINGS = 3,
    OPERATION_MAX = OPERATION_SETTINGS
  };

  // A struct that contains extra data about a notification specific to one of
  // the above types.
  struct Metadata {
    virtual ~Metadata();

    NotificationHandler::Type type;

    std::string web_app_id;
  };
};

// Metadata for PERSISTENT notifications.
struct PersistentNotificationMetadata : public NotificationCommon::Metadata {
  PersistentNotificationMetadata();
  ~PersistentNotificationMetadata() override;

  static const PersistentNotificationMetadata* From(const Metadata* metadata);

  GURL service_worker_scope;
};

// Metadata for NON_PERSISTENT notifications.
struct NonPersistentNotificationMetadata : public NotificationCommon::Metadata {
  NonPersistentNotificationMetadata();
  ~NonPersistentNotificationMetadata() override;

  static const NonPersistentNotificationMetadata* From(
      const Metadata* metadata);

  // This is empty when used for a worker.
  GURL document_url;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
