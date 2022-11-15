// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from ash/public/cpp/notifier_settings_controller.h

#ifndef NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_SETTINGS_CONTROLLER_H_
#define NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_SETTINGS_CONTROLLER_H_

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

class NotifierSettingsObserver;

// An interface, implemented by Chrome, which allows Ash to read and write
// settings and UI data regarding message center notification sources.
class NotifierSettingsController {
 public:
  // Returns the singleton instance.
  static NotifierSettingsController* Get();

  NotifierSettingsController(const NotifierSettingsController&) = delete;
  NotifierSettingsController& operator=(const NotifierSettingsController&) =
      delete;

  // Called to toggle the |enabled| state of a specific notifier (in response to
  // a user selecting or de-selecting that notifier).
  virtual void SetNotifierEnabled(const GURL& origin, bool enabled) = 0;
  virtual void ResetNotifier(const GURL& origin) = 0;

 protected:
  NotifierSettingsController();
  virtual ~NotifierSettingsController();
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_PUBLIC_NOTIFIER_SETTINGS_CONTROLLER_H_
