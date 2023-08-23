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

#include "neva/browser_shell/app/browser_shell_main_delegate.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "neva/app_runtime/app/app_runtime_shell.h"
#include "neva/app_runtime/browser/app_runtime_browser_main_parts.h"
#include "neva/app_runtime/browser/app_runtime_content_browser_client.h"
#include "neva/browser_shell/app/platform_language.h"
#include "neva/browser_shell/app/platform_registration.h"
#include "neva/browser_shell/common/browser_shell_switches.h"
#include "neva/browser_shell/service/browser_shell_service_impl.h"
#include "neva/browser_shell/service/public/browser_shell_service.h"
#include "neva/injection/public/common/webapi_names.h"
#include "url/gurl.h"

namespace browser_shell {

namespace {

const std::string kFullscreenKey = "fullscreen";
const std::string kUserAgentKey = "user-agent";

std::string ReadLaunchArgs() {
  std::string args_from_cli = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kShellLaunchArgs);

  base::Value::Dict dict;
  absl::optional<base::Value> json = base::JSONReader::Read(args_from_cli);
  if (json && json->is_dict())
    dict = std::move(json->GetDict());

  std::string* user_agent_value = dict.FindString(kUserAgentKey);
  if (!user_agent_value) {
    std::string user_agent_from_cli = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(embedder_support::kUserAgent);
    if (!user_agent_from_cli.empty())
      dict.Set(kUserAgentKey, user_agent_from_cli);
  }

  absl::optional<bool> fullscreen_value = dict.FindBool(kFullscreenKey);
  if (!fullscreen_value) {
    bool fullscreen = base::CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kShellFullscreen);
    if (fullscreen)
      dict.Set(kFullscreenKey, fullscreen);
  }

  std::string result;
  base::JSONWriter::Write(base::Value(std::move(dict)), &result);
  return result;
}

}  // namespace

BrowserShellMainDelegate::BrowserShellMainDelegate(
    content::MainFunctionParams parameters)
    : parameters_(std::move(parameters)) {}

BrowserShellMainDelegate::~BrowserShellMainDelegate() = default;

void BrowserShellMainDelegate::PreMainMessageLoopRun() {
  std::string path = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kShellAppPath);
  GURL url(path);
  if (!url.is_valid())
    LOG(ERROR) << "shell-app-path switch has invalid url: " << path;

  const bool enable_dev_tools =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRemoteDebuggingPort);

  if (enable_dev_tools) {
    // TODO(pikulik): That is how it's done in wam_demo and app_runtime
    // now. I think we should to revise the method we get access to
    // AppRuntimeBrowserMainParts.
    neva_app_runtime::AppRuntimeBrowserMainParts* main_parts =
        static_cast<neva_app_runtime::AppRuntimeBrowserMainParts*>(
            neva_app_runtime::GetAppRuntimeContentBrowserClient()
                ->GetMainParts());
    if (main_parts)
      main_parts->EnableDevTools();
  }

  std::vector<std::string> api_list;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShellWebAPIs)) {
    std::string apis = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kShellWebAPIs);

    if (!apis.empty())
      api_list = base::SplitString(apis,
                                   ",",
                                   base::WhitespaceHandling::TRIM_WHITESPACE,
                                   base::SplitResult::SPLIT_WANT_NONEMPTY);
  }
#if defined(USE_NEVA_CHROME_EXTENSIONS)
  api_list.push_back(injections::webapi::kChromeExtensions);
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

  bool fullscreen = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShellFullscreen);

  neva_app_runtime::Shell::CreateParams shell_params;
  shell_params.app_id = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kShellAppId);
  shell_params.launch_params = ReadLaunchArgs();
  shell_params.enable_dev_tools = enable_dev_tools;

  auto shell = std::make_unique<neva_app_runtime::Shell>(shell_params);
  auto* main_window = shell->CreateMainWindow(url.spec(), api_list, fullscreen);

  platform_language_ = std::make_unique<PlatformLanguage>(main_window);
  shell->AddObserver(platform_language_.get());

  platform_registration_ = std::make_unique<PlatformRegistration>(main_window);
  shell->AddObserver(platform_registration_.get());

  RegisterShellService(std::make_unique<ShellServiceImpl>(std::move(shell)));
}

void BrowserShellMainDelegate::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  neva_app_runtime::Shell::SetQuitClosure(run_loop->QuitClosure());
}

}  // namespace browser_shell
