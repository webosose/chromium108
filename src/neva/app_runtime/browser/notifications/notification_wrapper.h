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

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_WRAPPER_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_WRAPPER_H_

#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "neva/app_runtime/public/notification.h"
#include "ui/message_center/public/cpp/notification.h"

namespace neva_app_runtime {

class NotificationWrapper : public Notification {
 public:
  NotificationWrapper(const message_center::Notification& notification,
                      std::unique_ptr<NotificationCommon::Metadata> metadata);
  ~NotificationWrapper() override {}

  // Notification implementation
  const std::string& Id() const override;
  const std::string& Origin() const override;
  const std::u16string& Title() const override;
  const std::u16string& Message() const override;
  std::string AppId() const override;
  const std::vector<ButtonInfo>& Buttons() const override;

 private:
  message_center::Notification notification_;
  std::unique_ptr<NotificationCommon::Metadata> metadata_;
  std::vector<ButtonInfo> buttons_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_WRAPPER_H_
