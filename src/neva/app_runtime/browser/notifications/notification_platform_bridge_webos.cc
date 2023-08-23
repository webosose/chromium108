// Copyright (c) 2022 LG Electronics, Inc.
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

#include "neva/app_runtime/browser/notifications/notification_platform_bridge_webos.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "neva/pal_service/pal_service.h"
#include "ui/message_center/public/cpp/notification.h"

namespace neva_app_runtime {

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  return std::make_unique<NotificationPlatformBridgeWebos>();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

NotificationPlatformBridgeWebos::NotificationPlatformBridgeWebos() = default;

NotificationPlatformBridgeWebos::~NotificationPlatformBridgeWebos() = default;

void NotificationPlatformBridgeWebos::Display(
    NotificationHandler::Type notification_type,
    content::BrowserContext* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  if (metadata->web_app_id.empty()) {
    LOG(INFO) << __func__ << " is called with the empty app_id";
    return;
  }

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("sourceId", metadata->web_app_id);
  std::string message = "[" + base::UTF16ToUTF8(notification.title()) + "]";
  if (!notification.message().empty()) {
    message += " " + base::UTF16ToUTF8(notification.message());
  }
  dict.SetStringKey("message", message);
  dict.SetStringKey("type", "advanced");
  dict.SetBoolKey("persistent", true);
  std::string param;
  base::JSONWriter::Write(dict, &param);

  if (!remote_system_bridge_) {
    mojo::Remote<pal::mojom::SystemServiceBridgeProvider> provider;
    pal::GetPalService(content::GetUIThreadTaskRunner({}))
        .BindSystemServiceBridgeProvider(provider.BindNewPipeAndPassReceiver());

    provider->GetSystemServiceBridge(
        remote_system_bridge_.BindNewPipeAndPassReceiver());

    auto params = pal::mojom::ConnectionParams::New(
        absl::make_optional<std::string>(),
        absl::make_optional<std::string>(metadata->web_app_id), -1);

    remote_system_bridge_->Connect(
        std::move(params),
        base::BindOnce(
            [](base::WeakPtr<NotificationPlatformBridgeWebos> bridge,
               std::string payload,
               mojo::PendingAssociatedReceiver<
                   pal::mojom::SystemServiceBridgeClient> client) {
              if (bridge)
                bridge->DisplayInternal(payload);
            },
            weak_factory_.GetWeakPtr(), param));
    return;
  }

  DisplayInternal(param);
}

void NotificationPlatformBridgeWebos::DisplayInternal(
    const std::string& payload) {
  if (!remote_system_bridge_) {
    LOG(ERROR) << __func__ << " remote_system_bridge_ is not bound.";
    return;
  }

  remote_system_bridge_->Call("luna://com.webos.notification/createToast",
                              payload);
}

void NotificationPlatformBridgeWebos::Close(
    content::BrowserContext* profile,
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeWebos::GetDisplayed(
    content::BrowserContext* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeWebos::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeWebos::DisplayServiceShutDown(
    content::BrowserContext* profile) {
  NOTIMPLEMENTED();
}

}  // namespace neva_app_runtime
