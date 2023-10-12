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

#include "neva/app_runtime/browser/app_runtime_browser_main_parts.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/watchdog/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/result_codes.h"
#include "device/bluetooth/dbus/dbus_bluez_manager_wrapper_linux.h"
#include "net/base/network_change_notifier_factory.h"
#include "neva/app_runtime/browser/app_runtime_browser_context.h"
#include "neva/app_runtime/browser/app_runtime_browser_main_extra_parts.h"
#include "neva/app_runtime/browser/app_runtime_browser_switches.h"
#include "neva/app_runtime/browser/app_runtime_devtools_manager_delegate.h"
#include "neva/app_runtime/browser/app_runtime_shared_memory_manager.h"
#include "neva/app_runtime/browser/net/app_runtime_network_change_notifier.h"
#include "neva/app_runtime/browser/permissions/neva_permissions_client.h"
#include "ui/linux/linux_ui.h"
#include "ui/views/widget/desktop_aura/neva/views_delegate_stub.h"

#if defined(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

#if defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
#include "ozone/ui/webui/ozone_webui.h"
#endif  // defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // defined(USE_OZONE)

#if defined(USE_NEVA_CHROME_EXTENSIONS)
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id_generator.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#include "neva/app_runtime/browser/extensions/tab_helper_impl.h"
#include "neva/extensions/browser/browser_context_keyed_service_factories.h"
#include "neva/extensions/browser/neva_extensions_service_factory.h"
#include "neva/extensions/browser/neva_extensions_service_impl.h"
#include "neva/extensions/browser/neva_extensions_util.h"
#include "neva/extensions/browser/neva_prefs.h"
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

namespace neva_app_runtime {

namespace {

void InitializeUI() {
#if defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
  // Initialization of input method factory
  ui::LinuxUi::SetInstance(BuildWebUI());
#endif  // defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
}

bool IsWayland() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::IsWayland();
#else  // defined(USE_OZONE)
  return false;
#endif  // !defined(USE_OZONE)
}

bool IsWaylandExternal() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::IsWaylandExternal();
#else  // defined(USE_OZONE)
  return false;
#endif  // !defined(USE_OZONE)
}

}  // namespace

#if defined(OS_WEBOS)
constexpr char kProductName[] = "webOS";
#else   // defined(OS_WEBOS)
constexpr char kProductName[] = "neva";
#endif  // defined(OS_WEBOS)

class AppRuntimeNetworkChangeNotifierFactory
    : public net::NetworkChangeNotifierFactory {
 public:
  // net::NetworkChangeNotifierFactory overrides.
  std::unique_ptr<net::NetworkChangeNotifier> CreateInstance() override {
    return base::WrapUnique(new AppRuntimeNetworkChangeNotifier());
  }
};

AppRuntimeBrowserMainParts::AppRuntimeBrowserMainParts() : BrowserMainParts() {}

AppRuntimeBrowserMainParts::~AppRuntimeBrowserMainParts() {}

void AppRuntimeBrowserMainParts::AddParts(
    AppRuntimeBrowserMainExtraParts* parts) {
  app_runtime_extra_parts_.push_back(parts);
}

int AppRuntimeBrowserMainParts::DevToolsPort() const {
  return AppRuntimeDevToolsManagerDelegate::GetHttpHandlerPort();
}

void AppRuntimeBrowserMainParts::EnableDevTools() {
  if (dev_tools_enabled_)
    return;

  AppRuntimeDevToolsManagerDelegate::StartHttpHandler(
      GetDefaultBrowserContext());
  dev_tools_enabled_ = true;
}

void AppRuntimeBrowserMainParts::DisableDevTools() {
  if (!dev_tools_enabled_)
    return;

  AppRuntimeDevToolsManagerDelegate::StopHttpHandler();
  dev_tools_enabled_ = false;
}

int AppRuntimeBrowserMainParts::PreEarlyInitialization() {
  if (IsWaylandExternal())
    InitializeUI();
  else if (!IsWayland())
    // Only for testing. As stub for other platforms.
    ui::InitializeInputMethodForTesting();
  return RESULT_CODE_NORMAL_EXIT;
}

void AppRuntimeBrowserMainParts::ToolkitInitialized() {
  if (IsWaylandExternal()) {
    if (!ui::LinuxUi::instance()->Initialize())
      LOG(ERROR) << __func__ << " failed to initialize LinuxUi";
  }

  if (!views::ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<views::ViewsDelegateStub>();
}

void AppRuntimeBrowserMainParts::PreCreateMainMessageLoop() {
  // Replace the default NetworkChangeNotifierFactory with app runtime
  // implementation. This must be done before BrowserMainLoop calls
  // net::NetworkChangeNotifier::Create() in PostMainMessageLoopStart().
  net::NetworkChangeNotifier::SetFactory(
      new neva_app_runtime::AppRuntimeNetworkChangeNotifierFactory());
}

void AppRuntimeBrowserMainParts::PostCreateMainMessageLoop() {
  app_runtime_mem_manager_.reset(new AppRuntimeSharedMemoryManager);
  bluez::DBusBluezManagerWrapperLinux::Initialize();
#if defined(USE_OZONE)
  auto shutdown_cb = base::BindOnce([] {
    base::Process::TerminateCurrentProcessImmediately(1);
    LOG(FATAL) << "Browser failed to shutdown.";
  });
  ui::OzonePlatform::GetInstance()->PostCreateMainMessageLoop(
      std::move(shutdown_cb));
#endif  // defined(USE_OZONE)

  // Set up crypt config. This needs to be done before anything starts the
  // network service, as the raw encryption key needs to be shared with the
  // network service for encrypted cookie storage.
  CreateOSCryptConfig();
}

int AppRuntimeBrowserMainParts::PreMainMessageLoopRun() {
#if defined(USE_NEVA_CHROME_EXTENSIONS)
  // Extensions clients should be created before KeyedServiceFactories creation.
  extensions_client_ = std::make_unique<neva::NevaExtensionsClient>();
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_ =
      std::make_unique<neva::NevaExtensionsBrowserClient>();
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  // All KeyedServiceFactories must be created before context is created.
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  neva::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

  auto* browser_context = GetDefaultBrowserContext();
  std::ignore = browser_context;

#if defined(ENABLE_PLUGINS)
  plugin_service_filter_.reset(new AppRuntimePluginServiceFilter);
  content::PluginService::GetInstance()->SetFilter(
      plugin_service_filter_.get());
#endif

#if defined(USE_AURA)
  if (!display::Screen::GetScreen()) {
    screen_ = views::CreateDesktopScreen();
    display::Screen::SetScreenInstance(screen_.get());
  }

  aura::Env::GetInstance();
#endif

#if defined(USE_NEVA_CHROME_EXTENSIONS)
  // TODO(pikulik): Looks like it's better to move the following code to
  // AppRuntimeBrowserContext. This code initialize Extensions support for
  // default/main BrowserContext only. But addtional PageContents/WebContents
  // can be created with another browsing session i.e. BrowserContext.

  // app_shell only supports a single user, so all preferences live in the user
  // data directory, including the device-wide local state.
  local_state_ = neva::prefs::CreateLocalState(browser_context->GetPath());
  sessions::SessionIdGenerator::GetInstance()->Init(local_state_.get());
  user_pref_service_ = neva::prefs::CreateUserPrefService(browser_context);
  extensions_browser_client_->InitWithBrowserContext(browser_context,
                                                     user_pref_service_.get());

  extension_system_ = static_cast<neva::NevaExtensionSystem*>(
      extensions::ExtensionSystem::Get(browser_context));
  extension_system_->InitForRegularProfile(true /* extensions_enabled */);
  extension_system_->FinishInitialization();

  neva::LoadExtensionsFromCommandLine(extension_system_);
  neva::NevaExtensionsServiceFactory::GetService(browser_context)
      ->SetTabHelper(std::make_unique<neva::TabHelperImpl>());
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

  for (auto* extra_part : app_runtime_extra_parts_)
    extra_part->PreMainMessageLoopRun();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(watchdog::switches::kEnableWatchdog)) {
    ui_watchdog_.reset(new watchdog::Watchdog());
    io_watchdog_.reset(new watchdog::Watchdog());

    std::string env_timeout = command_line->GetSwitchValueASCII(
        watchdog::switches::kWatchdogBrowserTimeout);
    if (!env_timeout.empty()) {
      int timeout;
      if (base::StringToInt(env_timeout, &timeout)) {
        ui_watchdog_->SetTimeout(timeout);
        io_watchdog_->SetTimeout(timeout);
      }
    }

    std::string env_period = command_line->GetSwitchValueASCII(
        watchdog::switches::kWatchdogBrowserPeriod);
    if (!env_period.empty()) {
      int period;
      if (base::StringToInt(env_period, &period)) {
        ui_watchdog_->SetPeriod(period);
        io_watchdog_->SetPeriod(period);
      }
    }

    ui_watchdog_->StartWatchdog();
    io_watchdog_->StartWatchdog();

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppRuntimeBrowserMainParts::ArmWatchdog,
                       base::Unretained(this), content::BrowserThread::UI,
                       ui_watchdog_.get()));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppRuntimeBrowserMainParts::ArmWatchdog,
                       base::Unretained(this), content::BrowserThread::IO,
                       io_watchdog_.get()));
  }
  return RESULT_CODE_NORMAL_EXIT;
}

void AppRuntimeBrowserMainParts::WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) {
  for (auto* extra_part : app_runtime_extra_parts_)
    extra_part->WillRunMainMessageLoop(run_loop);
}

void AppRuntimeBrowserMainParts::ArmWatchdog(content::BrowserThread::ID thread,
                                             watchdog::Watchdog* watchdog) {
  watchdog->Arm();
  if (!watchdog->HasThreadInfo())
    watchdog->SetCurrentThreadInfo();

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppRuntimeBrowserMainParts::ArmWatchdog,
                     base::Unretained(this), thread, watchdog),
      base::Seconds(watchdog->GetPeriod()));
}

int AppRuntimeBrowserMainParts::PreCreateThreads() {
  // Make sure permissions client has been set.
  NevaPermissionsClient::GetInstance();
  return content::RESULT_CODE_NORMAL_EXIT;
}

void AppRuntimeBrowserMainParts::PostCreateThreads() {
  heap_profiling::Mode mode = heap_profiling::GetModeForStartup();
  if (mode != heap_profiling::Mode::kNone)
    heap_profiling::Supervisor::GetInstance()->Start(base::NullCallback());
}

void AppRuntimeBrowserMainParts::PostMainMessageLoopRun() {
  DisableDevTools();
}

void AppRuntimeBrowserMainParts::PostDestroyThreads() {
  bluez::DBusBluezManagerWrapperLinux::Shutdown();
}

content::BrowserContext*
AppRuntimeBrowserMainParts::GetDefaultBrowserContext() const {
  return AppRuntimeBrowserContext::From("");
}

void AppRuntimeBrowserMainParts::CreateOSCryptConfig() {
  std::unique_ptr<os_crypt::Config> config =
      std::make_unique<os_crypt::Config>();
  // Forward to os_crypt the flag to use a specific password store.
  config->store = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      kPasswordStore);
  // Use the product name
  config->product_name = kProductName;
  // OSCrypt may target keyring, which requires calls from the main thread.
  config->main_thread_runner = content::GetUIThreadTaskRunner({});
  // OSCrypt can be disabled in a special settings file.
  config->should_use_preference = false;
  config->user_data_path = base::FilePath();
  OSCrypt::SetConfig(std::move(config));
}

}  // namespace neva_app_runtime
