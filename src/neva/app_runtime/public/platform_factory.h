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
#ifndef NEVA_APP_RUNTIME_PLATFORM_FACTORY_H_
#define NEVA_APP_RUNTIME_PLATFORM_FACTORY_H_

#include <memory>

#include "neva/app_runtime/public/notification_platform_bridge.h"
#include "neva/app_runtime/public/permission_prompt.h"

namespace neva_app_runtime {

class PlatformFactory;
PlatformFactory* GetPlatformFactory();
void SetPlatformFactory(PlatformFactory*);

class PlatformFactory {
 public:
  virtual ~PlatformFactory() {}
  virtual std::unique_ptr<NotificationPlatformBridge>
  CreateNotificationPlatformBridge() = 0;
  virtual std::unique_ptr<PermissionPrompt> CreatePermissionPrompt(
      PermissionPrompt::Delegate* delegate) = 0;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_PLATFORM_FACTORY_H_
