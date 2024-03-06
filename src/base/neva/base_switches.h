// Copyright 2018 LG Electronics, Inc.
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

#ifndef BASE_NEVA_BASE_SWITCHES_H_
#define BASE_NEVA_BASE_SWITCHES_H_

#include "base/base_export.h"

namespace switches {

BASE_EXPORT extern const char kV8SnapshotBlobPath[];

BASE_EXPORT extern const char kDecodedImageWorkingSetBudgetMB[];
BASE_EXPORT extern const char kEnableNotificationForUnsupportedFeatures[];

BASE_EXPORT extern const char kLocalStorageLimitPerSecondLevelDomain[];
BASE_EXPORT extern const char kNevaCertificatesPath[];
BASE_EXPORT extern const char kPwaInstallPath[];

}  // namespace switches

#endif  // BASE_NEVA_BASE_SWITCHES_H_
