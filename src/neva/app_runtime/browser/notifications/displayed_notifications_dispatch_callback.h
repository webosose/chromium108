// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //chrome/browser/notifications/displayed_notifications_dispatch_callback.h.

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_

#include <set>
#include <string>

#include "base/callback.h"

namespace neva_app_runtime {

// Callback used by the bridge and all the downstream classes that propagate
// the callback to get displayed notifications.
//
// |supports_synchronization| will be true if the platform supports getting the
// currently displayed notifications.
//
// If |supports_synchronization| is true, then |notification_ids| will contain
// the ids of the currently displayed notifications, otherwise the value of
// |notification_ids| should be ignored.
using GetDisplayedNotificationsCallback =
    base::OnceCallback<void(std::set<std::string> notification_ids,
                            bool supports_synchronization)>;

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
