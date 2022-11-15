// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/notifications/web_page_notifier_controller.h

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS__WEB_PAGE_NOTIFIER_CONTROLLER_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS__WEB_PAGE_NOTIFIER_CONTROLLER_H_

#include "neva/app_runtime/browser/notifications/notifier_controller.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

class WebPageNotifierController : public NotifierController {
 public:
  explicit WebPageNotifierController();
  ~WebPageNotifierController() override;

  void SetNotifierEnabled(content::BrowserContext* browser_context,
                          const GURL& origin,
                          bool enabled) override;
  void ResetNotifier(content::BrowserContext* browser_context,
                     const GURL& origin) override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_WEB_PAGE_NOTIFIER_CONTROLLER_H_
