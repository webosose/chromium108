// Copyright 2022 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFIER_CLIENT_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFIER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "neva/app_runtime/public/notifier_settings_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace neva_app_runtime {

class NotifierController;

class NotifierClient : public NotifierSettingsController {
 public:
  explicit NotifierClient(content::BrowserContext* browser_context);
  NotifierClient(const NotifierClient&) = delete;
  NotifierClient& operator=(const NotifierClient&) = delete;
  ~NotifierClient() override;

  // NotifierSettingsController:
  void SetNotifierEnabled(const GURL& url_string, bool enabled) override;
  void ResetNotifier(const GURL& url_string) override;

 private:
  // Notifier source for web page
  std::unique_ptr<NotifierController> web_page_source_;
  content::BrowserContext* browser_context_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFIER_CLIENT_H_
