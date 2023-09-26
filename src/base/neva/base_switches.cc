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

#include "base/neva/base_switches.h"

namespace switches {

// V8 snapshot blob path
const char kV8SnapshotBlobPath[] = "v8-snapshot-blob-path";

// Layer tree setting for decoded image working set budget in MB
const char kDecodedImageWorkingSetBudgetMB[] =
    "decoded-image-working-set-budget-mb";

// When using unsupported feature in app_runtime (e.g. File download, upload),
// notification occurs.
const char kEnableNotificationForUnsupportedFeatures[] =
    "enable-notification-for-unsupported-features";

// Limits size of local storage for the second level domain in MB
const char kLocalStorageLimitPerSecondLevelDomain[] =
    "local-storage-limit-per-second-level-domain";

const char kNevaCertificatesPath[] = "neva-certificates-path";

const char kPwaInstallPath[] = "pwa-install-path";

}  // namespace switches
