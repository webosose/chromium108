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

#include "neva/browser_shell/app/platform_registration.h"

#include "base/command_line.h"
#include "neva/app_runtime/app/app_runtime_shell_window.h"
#include "neva/browser_shell/common/browser_shell_switches.h"
#include "neva/pal_service/pal_platform_factory.h"
#include "neva/pal_service/public/application_registrator_delegate.h"

namespace browser_shell {

PlatformRegistration::PlatformRegistration(
    neva_app_runtime::ShellWindow* main_window)
    : main_window_(main_window) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kShellAppId)) {
    std::string name = cmd->GetSwitchValueASCII(switches::kShellAppId);
    delegate_ =
        pal::PlatformFactory::Get()->CreateApplicationRegistratorDelegate(
            std::move(name),
            base::BindRepeating(&PlatformRegistration::OnMessage,
                                base::Unretained(this)));
    if (!delegate_) {
      LOG(ERROR) << __func__ << "(): Application registration is not supported";
      return;
    }

    if (delegate_->GetStatus() !=
        pal::ApplicationRegistratorDelegate::Status::kSuccess) {
      LOG(ERROR) << __func__ << "(): Application " << name
                 << " was not registered.";
    }
  }
}

PlatformRegistration::~PlatformRegistration() = default;

void PlatformRegistration::OnMessage(const std::string& message) {
  if (main_window_ && (message == "relaunch"))
    main_window_->ToggleFullscreen();
}

void PlatformRegistration::OnMainWindowClosing() {
  main_window_ = nullptr;
}

}  // namespace browser_shell
