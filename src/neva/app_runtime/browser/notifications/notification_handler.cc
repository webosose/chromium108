// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_handler.cc.

#include "neva/app_runtime/browser/notifications/notification_handler.h"

#include "base/callback.h"

namespace neva_app_runtime {

NotificationHandler::~NotificationHandler() = default;

void NotificationHandler::OnShow(content::BrowserContext* profile,
                                 const std::string& notification_id) {}

void NotificationHandler::OnClose(content::BrowserContext* profile,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  bool by_user,
                                  base::OnceClosure completed_closure) {
  std::move(completed_closure).Run();
}

void NotificationHandler::OnClick(content::BrowserContext* profile,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  const absl::optional<int>& action_index,
                                  const absl::optional<std::u16string>& reply,
                                  base::OnceClosure completed_closure) {
  std::move(completed_closure).Run();
}

void NotificationHandler::DisableNotifications(content::BrowserContext* profile,
                                               const GURL& origin) {
  NOTREACHED();
}

void NotificationHandler::OpenSettings(content::BrowserContext* profile,
                                       const GURL& origin) {
  // Notification types that display a settings button must override this method
  // to handle user interaction with it.
  NOTREACHED();
}

}  // namespace neva_app_runtime
