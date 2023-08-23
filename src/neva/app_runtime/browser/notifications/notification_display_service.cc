// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_display_service.cc.

#include "neva/app_runtime/browser/notifications/notification_display_service.h"

#include "neva/app_runtime/browser/notifications/notification_display_service_factory.h"

namespace neva_app_runtime {

// static
NotificationDisplayService* NotificationDisplayService::GetForProfile(
    content::BrowserContext* profile) {
  return NotificationDisplayServiceFactory::GetForProfile(profile);
}

NotificationDisplayService::~NotificationDisplayService() = default;

}  // namespace neva_app_runtime
