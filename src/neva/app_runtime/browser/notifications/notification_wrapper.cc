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

#include "neva/app_runtime/browser/notifications/notification_wrapper.h"

#include <codecvt>
#include <locale>

#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "ui/message_center/public/cpp/notification.h"

namespace neva_app_runtime {

NotificationWrapper::NotificationWrapper(
    const message_center::Notification& notification)
    : notification_(notification) {
  if (!notification_.buttons().empty()) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    for (const auto& button : notification_.buttons()) {
      buttons_.emplace_back(conv.to_bytes(button.title), std::string());
    }
  }
}

const std::string& NotificationWrapper::Id() const {
  return notification_.id();
}

const std::string& NotificationWrapper::Origin() const {
  return notification_.origin_url().spec();
}

const std::u16string& NotificationWrapper::Title() const {
  return notification_.title();
}

const std::u16string& NotificationWrapper::Message() const {
  return notification_.message();
}

std::string NotificationWrapper::AppId() const {
  if (notification_.origin_url().get_webapp_id()) {
    return notification_.origin_url().get_webapp_id().value();
  }
  return std::string();
}

const std::vector<ButtonInfo>& NotificationWrapper::Buttons() const {
  return buttons_;
}

}  // namespace neva_app_runtime
