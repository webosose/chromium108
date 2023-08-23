// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_display_service.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "neva/app_runtime/browser/notifications/notification_handler.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace message_center {
class Notification;
}

namespace neva_app_runtime {

// Profile-bound service that enables user-visible notifications to be displayed
// and managed. Notifications may either be presented using a notification
// center provided by the platform, or by Chrome's Message Center.
class NotificationDisplayService : public KeyedService {
 public:
  NotificationDisplayService(const NotificationDisplayService&) = delete;
  NotificationDisplayService& operator=(const NotificationDisplayService&) =
      delete;
  ~NotificationDisplayService() override;

  // Callback to be used with the GetDisplayed() method. Includes the set of
  // notification ids that is being displayed to the user. The
  // |supports_synchronization| callback indicates whether the platform has the
  // ability to query which notifications are still being displayed.
  //
  // TODO(peter): Rename |supports_synchronization| to |supported|.
  using DisplayedNotificationsCallback =
      base::OnceCallback<void(std::set<std::string>,
                              bool /* supports_synchronization */)>;

  // Returns an instance of the display service for the given |profile|.
  static NotificationDisplayService* GetForProfile(
      content::BrowserContext* profile);

  // Displays the |notification| of type |notification_type|. The |metadata|
  // may be provided for certain notification types that require additional
  // information for the notification to be displayed.
  virtual void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) = 0;

  // Closes the notification having |notification_id| of |notification_type|.
  virtual void Close(NotificationHandler::Type notification_type,
                     const std::string& notification_id) = 0;

  // Gets the IDs of currently displaying notifications and invokes |callback|
  // once available. Not all backends support retrieving this information.
  virtual void GetDisplayed(DisplayedNotificationsCallback callback) = 0;

 protected:
  NotificationDisplayService() = default;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
