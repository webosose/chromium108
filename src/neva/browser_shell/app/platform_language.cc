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

#include "neva/browser_shell/app/platform_language.h"

#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_page_view.h"
#include "neva/app_runtime/app/app_runtime_shell_window.h"
#include "neva/pal_service/pal_platform_factory.h"
#include "neva/pal_service/public/language_tracker_delegate.h"

namespace browser_shell {

PlatformLanguage::PlatformLanguage(neva_app_runtime::ShellWindow* main_window)
    : main_window_(main_window),
      delegate_(pal::PlatformFactory::Get()->CreateLanguageTrackerDelegate(
          base::BindRepeating(&PlatformLanguage::OnLanguageChanged,
                              base::Unretained(this)))) {}

PlatformLanguage::~PlatformLanguage() = default;

void PlatformLanguage::OnLanguageChanged(const std::string& language_string) {
  if (main_window_) {
    auto* page_view = main_window_->GetPageView();
    if (!page_view)
      return;

    auto* page_contents = page_view->GetPageContents();
    if (!page_contents)
      return;

    page_contents->UpdatePreferredLanguage(language_string);
  }
}

void PlatformLanguage::OnMainWindowClosing() {
  main_window_ = nullptr;
}

}  // namespace browser_shell
