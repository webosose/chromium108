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

#include "neva/app_runtime/browser/notifications/notifier_client.h"

#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/notifications/web_page_notifier_controller.h"
#include "neva/app_runtime/webview_profile.h"
#include "url/gurl.h"

namespace neva_app_runtime {

NotifierClient::NotifierClient(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  web_page_source_ = std::make_unique<WebPageNotifierController>();
}

NotifierClient::~NotifierClient() = default;

void NotifierClient::SetNotifierEnabled(const GURL& origin, bool enabled) {
  VLOG(1) << __func__ << " origin: " << origin << ", enabled: " << enabled;
  web_page_source_->SetNotifierEnabled(browser_context_, origin, enabled);
}

void NotifierClient::ResetNotifier(const GURL& origin) {
  VLOG(1) << __func__ << " origin: " << origin;
  web_page_source_->ResetNotifier(browser_context_, origin);
}

}  // namespace neva_app_runtime
