// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/switches.h"

#include "build/chromeos_buildflags.h"

namespace extensions {
namespace switches {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Allow roaming in the cellular network.
const char kAppShellAllowRoaming[] = "app-shell-allow-roaming";

// Size for the host window to create (i.e. "800x600").
const char kAppShellHostWindowSize[] = "app-shell-host-window-size";

// SSID of the preferred WiFi network.
const char kAppShellPreferredNetwork[] = "app-shell-preferred-network";
#endif

#if defined(USE_NEVA_APPRUNTIME)
const char kLaunchArgs[] = "launch-args";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";
#endif

}  // namespace switches
}  // namespace extensions
