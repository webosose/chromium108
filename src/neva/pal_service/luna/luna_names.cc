// Copyright 2019 LG Electronics, Inc.
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

#include "neva/pal_service/luna/luna_names.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/rand_util.h"

#include <stdlib.h>
#include <unistd.h>

namespace pal {
namespace luna {

namespace {

const char kLunaScheme[] = "luna://";
const char KPalmScheme[] = "palm://";

}  // namespace

namespace service_uri {

const char kAudio[] = "com.webos.audio";
const char kSettings[] = "com.webos.settingsservice";
const char kMemoryManager[] = "com.webos.memorymanager";
const char kPalmBus[] = "com.palm.bus";
const char kApplicationManager[] = "com.webos.applicationManager";
const char kServiceMemoryManager[] = "com.webos.service.memorymanager";
const char kNotification[] = "com.webos.notification";
const char kServiceBus[] = "com.webos.service.bus";
#if defined(ENABLE_PWA_MANAGER_WEBAPI)
const char kAppInstallService[] = "com.webos.appInstallService";
#endif  // ENABLE_PWA_MANAGER_WEBAPI

}  // namespace service_uri

namespace service_name {

#if defined(ENABLE_PWA_MANAGER_WEBAPI)
const char kChromiumInstallableManager[] =
    "com.webos.chromium.installablemanager";
const char kChromiumPwa[] = "com.webos.chromium.pwa";
#endif  // ENABLE_PWA_MANAGER_WEBAPI
const char kChromiumMedia[] = "com.webos.chromium.media";
const char kChromiumMemory[] = "com.webos.chromium.memory";
const char kChromiumPlatformSystem[] = "com.webos.chromium.platform.system";
const char kNotificationClient[] = "com.webos.notification.client";
const char kSettingsClient[] = "com.webos.settingsservice.client";

}  // namespace service_name

std::string GetServiceURI(const char* uri, const char* action) {
  CHECK(uri && action);
  std::string result(kLunaScheme);
  return result.append(uri).append("/").append(action);
}

std::string GetServiceName(const char* name,
                           int suffix,
                           const char* delimiter) {
  CHECK(name && delimiter);
  std::string result(name);
  return result.append(delimiter).append(std::to_string(suffix));
}

std::string GetServiceNameWithRandSuffix(const char* name,
                                         const char* delimiter) {
  return GetServiceName(name, base::RandInt(10000, 99999), delimiter);
}

std::string GetServiceNameWithPID(const char* name,
                                  const char* delimiter) {
  return GetServiceName(name, getpid(), delimiter);
}

}  // namespace luna
}  // namespace pal
