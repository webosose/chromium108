// Copyright 2021 LG Electronics, Inc.
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

#ifndef NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_H_
#define NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_shell_window_delegate.h"
#include "neva/browser_shell/common/browser_shell_export.h"

namespace neva_app_runtime {
class ShellObserver;
class ShellWindow;

class APP_RUNTIME_EXPORT Shell : public neva_app_runtime::ShellWindowDelegate {
 public:
  struct CreateParams {
    std::string app_id;
    std::string launch_params;
    bool enable_dev_tools = false;
  };

  explicit Shell(const CreateParams& params);
  ~Shell() override;

  void AddObserver(ShellObserver* observer);
  void RemoveObserver(ShellObserver* observer);

  ShellWindow* CreateMainWindow(std::string url,
                                const std::vector<std::string>& injections,
                                bool fullscreen);
  ShellWindow* GetMainWindow();
  PageContents::CreateParams GetDefaultContentsParams();
  void OnWindowClosing() override;

  const std::string& GetLaunchParams() const;

  static void SetQuitClosure(base::OnceClosure quit_main_message_loop);
  static void Shutdown();

 private:
  const std::string app_id_;
  const std::string launch_params_;
  base::ObserverList<ShellObserver> observers_;
  bool enable_dev_tools_;
  std::set<ShellWindow*> windows_;
  ShellWindow* main_window_ = nullptr;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_H_
