// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/notifications/web_page_notifier_controller.cc

#include "neva/app_runtime/browser/notifications/web_page_notifier_controller.h"

#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/notifications/notification_permission_context.h"
#include "url/gurl.h"

namespace neva_app_runtime {
WebPageNotifierController::WebPageNotifierController() {}
WebPageNotifierController::~WebPageNotifierController() {}

void WebPageNotifierController::SetNotifierEnabled(
    content::BrowserContext* browser_context,
    const GURL& origin,
    bool enabled) {
  if (origin.is_valid()) {
    NotificationPermissionContext::UpdatePermission(
        browser_context, origin,
        enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  } else {
    LOG(ERROR) << "Invalid pattern: " << origin.possibly_invalid_spec();
  }
}

void WebPageNotifierController::ResetNotifier(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  if (origin.is_valid()) {
    NotificationPermissionContext::UpdatePermission(browser_context, origin,
                                                    CONTENT_SETTING_DEFAULT);
  } else {
    LOG(ERROR) << "Invalid pattern: " << origin.possibly_invalid_spec();
  }
}

}  // namespace neva_app_runtime
