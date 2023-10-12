// Copyright 2016 LG Electronics, Inc.
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

#include "neva/app_runtime/browser/app_runtime_browser_switches.h"

// Allow file:// access if specified
const char kAllowFileAccess[] = "allow-file-access";

// Disables modern SameSite cookie behavior. This is the opposite of
// kWebViewEnableModernCookieSameSite flag for android. This disables the
// same-site-by-default-cookies, cookies-without-same-site-must-be-secure,
// and schemeful-same-site features.
const char kDisableModernCookieSameSite[] = "disable-modern-cookie-same-site";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[] = "disk-cache-size";

// The GIT revision a target was build from
const char kGitRevision[] = "git-revision";

// Use a specific location for the directory which contains data specific
// to a given user.
const char kUserDataDir[] = "user-data-dir";

// If true devtools experimental settings are enabled
const char kEnableDevToolsExperiments[] = "enable-devtools-experiments";

// Configure the portion of the pool size that can be utilized by a single host
// for temporary storage
const char kPerHostQuotaRatio[] = "per-host-quota-ratio";

// Configure quota pool size ratio for temporary storage such as indexeddb
const char kQuotaPoolSizeRatio[] = "quota-pool-size-ratio";

// Minimal memory limit to apply in face of moderate to critical memory
// pressure in MB.
const char kSharedMemMinimalLimitMB[] = "shared-mem-minimal-limit-mb";

// Divider factor to apply when calculating the memory limit in face of
// moderate memory pressure.
const char kSharedMemPressureDivider[] = "shared-mem-pressure-divider";

// Reduction factor to apply to overall system memory when calculating the
// default memory limit to use for discardable memory.
const char kSharedMemSystemMemReductionFactor[] =
    "shared-mem-system-mem-reduction-factor";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, gnome, gnome-keyring, gnome-libsecret, basic. Any other
// value will lead to Chrome detecting the best backend automatically.
// TODO(crbug.com/571003): Once PasswordStore no longer uses the Keyring or
// KWallet for storing passwords, rename this flag to stop referencing
// passwords. Do not rename it sooner, though; developers and testers might
// rely on it keeping large amounts of testing passwords out of their Keyrings
// or KWallets.
const char kPasswordStore[] = "password-store";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";

// Disable DropAllPeerConnections call for WebRTC
const char kDisableDropAllPeerConnections[] =
    "disable-drop-all-peer-connections";

// Configure default JavaScriptFlags and allow appruntime append on this flags.
// This flag is added because upstream is changed propagate command-line flags
// after added by content::ContentBrowserClient::AppendExtraCommandLineSwitches.
// (see https://chromium-review.googlesource.com/c/chromium/src/+/2910481)
const char kWebOSJavaScriptFlags[] = "webos-js-flags";
