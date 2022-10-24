// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/notifications/notifier_controller.h

#ifndef NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_CONTROLLER_H_
#define NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_CONTROLLER_H_

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

// An interface to control Notifiers, grouped by NotifierType. Controllers are
// responsible for both collating display data and toggling settings in response
// to user inputs.
class NotifierController {
 public:
  NotifierController() = default;
  NotifierController(const NotifierController&) = delete;
  NotifierController& operator=(const NotifierController&) = delete;
  virtual ~NotifierController() = default;

  // Set notifier enabled. |notifier_id| must have notifier type that can be
  // handled by the source. It has responsibility to invoke
  // Observer::OnNotifierEnabledChanged.
  virtual void SetNotifierEnabled(content::BrowserContext* browser_context,
                                  const GURL& url,
                                  bool enabled) = 0;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_CONTROLLER_H_
