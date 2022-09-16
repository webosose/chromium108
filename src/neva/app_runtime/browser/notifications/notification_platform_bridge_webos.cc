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
#include "neva/app_runtime/public/notification.h"
#include "neva/pal_service/pal_service.h"

namespace neva_app_runtime {

void NotificationPlatformBridgeWebos::Display(
    const Notification& notification) {
  if (notification.AppId().empty()) {
    LOG(INFO) << __func__ << " is called with the empty app_id";
    return;
  }

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("sourceId", notification.AppId());
  std::string message = "[" + base::UTF16ToUTF8(notification.Title()) + "]";
  if (!notification.Message().empty()) {
    message += " " + base::UTF16ToUTF8(notification.Message());
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
        absl::make_optional<std::string>(notification.AppId()), -1);

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
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeWebos::GetDisplayed(
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeWebos::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  NOTIMPLEMENTED();
}


}  // namespace neva_app_runtime
