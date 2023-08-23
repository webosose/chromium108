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

#ifndef NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WEBOS_H_
#define NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WEBOS_H_

#include <string>

#include "mojo/public/cpp/bindings/remote.h"
#include "neva/app_runtime/browser/notifications/notification_platform_bridge.h"
#include "neva/pal_service/public/mojom/system_servicebridge.mojom.h"

namespace neva_app_runtime {

class NotificationPlatformBridgeWebos : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeWebos();
  NotificationPlatformBridgeWebos(const NotificationPlatformBridgeWebos&) =
      delete;
  NotificationPlatformBridgeWebos& operator=(
      const NotificationPlatformBridgeWebos&) = delete;
  ~NotificationPlatformBridgeWebos() override;

  // NotificationPlatformBridge:
  void Display(NotificationHandler::Type notification_type,
               content::BrowserContext* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(content::BrowserContext* profile,
             const std::string& notification_id) override;
  void GetDisplayed(content::BrowserContext* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(content::BrowserContext* profile) override;

 private:
  void DisplayInternal(const std::string& params);

  mojo::Remote<pal::mojom::SystemServiceBridge> remote_system_bridge_;

  base::WeakPtrFactory<NotificationPlatformBridgeWebos> weak_factory_{this};
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WEBOS_H_
