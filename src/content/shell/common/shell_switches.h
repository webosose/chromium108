// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content_shell" command-line switches.

#ifndef CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
#define CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_

///@name USE_NEVA_APPRUNTIME
///@{
#include "content/shell/common/shell_neva_switches.h"
///@}

namespace switches {

extern const char kContentShellDataPath[];
extern const char kCrashDumpsDir[];
extern const char kDisableSystemFontCheck[];
extern const char kExposeInternalsForTesting[];
extern const char kContentShellHostWindowSize[];
extern const char kContentShellHideToolbar[];
extern const char kRunWebTests[];

// Helper that returns true if kRunWebTests is present in the command line,
// meaning Content Shell is running in web test mode.
bool IsRunWebTestsSwitchPresent();

}  // namespace switches

#endif  // CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
