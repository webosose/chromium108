// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/push_messaging/push_messaging_constants.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONSTANTS_H_
#define NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONSTANTS_H_

#include "base/time/time.h"

extern const char kPushMessagingGcmEndpoint[];

// The tag of the notification that will be automatically shown if a webapp
// receives a push message then fails to show a notification.
extern const char kPushMessagingForcedNotificationTag[];

// Chrome decided cadence on subscription refreshes. According to the standards:
// https://w3c.github.io/push-api/#dfn-subscription-expiration-time it is
// optional and set by the browser.
constexpr base::TimeDelta kPushSubscriptionExpirationPeriodTimeDelta =
    base::Days(90);

// TimeDelta for subscription refreshes to keep old subscriptions alive
constexpr base::TimeDelta kPushSubscriptionRefreshTimeDelta = base::Minutes(2);

#endif  // NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONSTANTS_H_
