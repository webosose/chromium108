// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/notification_platform_bridge.cc.

#include "neva/app_runtime/browser/notifications/notification_platform_bridge.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace neva_app_runtime {

// static
std::string NotificationPlatformBridge::GetProfileId(
    content::BrowserContext* profile) {
  if (!profile)
    return "";
#if defined(OS_WIN)
  return base::WideToUTF8(profile->GetPath().BaseName().value());
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  return profile->GetPath().BaseName().value();
#else
#error "Not implemented for !OS_WIN && !OS_POSIX."
#endif
}

}  // namespace neva_app_runtime
