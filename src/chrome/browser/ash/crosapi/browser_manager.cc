// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_action.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/ash/crosapi/files_app_launcher.h"
#include "chrome/browser/ash/crosapi/test_mojo_connection_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "chromeos/startup/startup_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "media/capture/capture_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/temporary_shared_resource_path_chromeos.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// TODO(crbug.com/1101667): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

// Resources file sharing mode.
enum class ResourcesFileSharingMode {
  kDefault = 0,
  // Failed to handle cached shared resources properly.
  kError = 1,
};

// The names of the UMA metrics to track Daily LaunchMode changes.
const char kLacrosLaunchModeDaily[] = "Ash.Lacros.Launch.Mode.Daily";
const char kLacrosLaunchModeAndSourceDaily[] =
    "Ash.Lacros.Launch.ModeAndSource.Daily";

// The interval at which the daily UMA reporting function should be
// called. De-duping of events will be happening on the server side.
constexpr base::TimeDelta kDailyLaunchModeTimeDelta = base::Minutes(30);

using LaunchParamsFromBackground = BrowserManager::LaunchParamsFromBackground;

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

// Global flag to disable most of BrowserManager for testing.
// Read by the BrowserManager constructor.
bool g_disabled_for_testing = false;

constexpr char kLacrosCannotLaunchNotificationID[] =
    "lacros_cannot_launch_notification_id";
constexpr char kLacrosLauncherNotifierID[] = "lacros_launcher";

base::FilePath LacrosLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

base::FilePath LacrosPreviousLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log.PREVIOUS");
}

// Moves any existing lacros log file to lacros.log.PREVIOUS. Returns true if a
// log file existed before being moved, and false if no log file was found.
bool RotateLacrosLogs() {
  // Remove lacros.log.PREVIOUS log entry if present.
  base::FilePath previous_log_path = LacrosPreviousLogPath();
  unlink(previous_log_path.value().c_str());

  base::FilePath log_path = LacrosLogPath();
  // Handle edge case where previous code created a symbolic link that could not
  // correctly resolve by deleting that symbolic link.
  if (base::IsLink(log_path)) {
    unlink(log_path.value().c_str());
    return true;
  }

  // If there is an existing log entry rename it to lacros.log.PREVIOUS.
  if (base::PathExists(log_path)) {
    base::Move(log_path, previous_log_path);
    return true;
  }

  return false;
}

ResourcesFileSharingMode ClearOrMoveSharedResourceFileInternal(
    bool clear_shared_resource_file,
    base::FilePath shared_resource_path) {
  // If shared resource pak doesn't exit, do nothing.
  if (!base::PathExists(shared_resource_path))
    return ResourcesFileSharingMode::kDefault;

  // Clear shared resource file cache if `clear_shared_resource_file` is true.
  if (clear_shared_resource_file) {
    if (!base::DeleteFile(shared_resource_path)) {
      LOG(ERROR) << "Failed to delete cached shared resource file.";
      return ResourcesFileSharingMode::kError;
    }
    return ResourcesFileSharingMode::kDefault;
  }

  base::FilePath renamed_shared_resource_path =
      ui::GetPathForTemporarySharedResourceFile(shared_resource_path);

  // Move shared resource pak to `renamed_shared_resource_path`.
  if (!base::Move(shared_resource_path, renamed_shared_resource_path)) {
    LOG(ERROR) << "Failed to move cached shared resource file to temporary "
               << "location.";
    return ResourcesFileSharingMode::kError;
  }
  return ResourcesFileSharingMode::kDefault;
}

ResourcesFileSharingMode ClearOrMoveSharedResourceFile(
    bool clear_shared_resource_file) {
  // Check 3 resource paks, resources.pak, chrome_100_percent.pak and
  // chrome_200_percent.pak.
  ResourcesFileSharingMode resources_file_sharing_mode =
      ResourcesFileSharingMode::kDefault;
  // Return kError if any of the resources failed to clear or move.
  // Make sure that ClearOrMoveSharedResourceFileInternal() runs for all
  // resources even if it already fails for some resource.
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file, browser_util::GetUserDataDir().Append(
                                          crosapi::kSharedResourcesPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file,
          browser_util::GetUserDataDir().Append(
              crosapi::kSharedChrome100PercentPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file,
          browser_util::GetUserDataDir().Append(
              crosapi::kSharedChrome200PercentPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  return resources_file_sharing_mode;
}

// This method runs some work on a background thread prior to launching lacros.
// The returns struct is used by the main thread as parameters to launch Lacros.
LaunchParamsFromBackground DoLacrosBackgroundWorkPreLaunch(
    base::FilePath lacros_dir,
    bool clear_shared_resource_file) {
  LaunchParamsFromBackground params;

  if (!RotateLacrosLogs()) {
    // If log file does not exist, most likely the user directory does not
    // exist either. So create it here.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(browser_util::GetUserDataDir(),
                                          &error)) {
      LOG(ERROR) << "Failed to make directory "
                 << browser_util::GetUserDataDir()
                 << base::File::ErrorToString(error);
      return params;
    }
  }

  int fd = HANDLE_EINTR(
      open(LacrosLogPath().value().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));

  if (fd < 0) {
    PLOG(ERROR) << "Failed to get file descriptor for " << LacrosLogPath();
    return params;
  }

  params.logfd = base::ScopedFD(fd);

  params.enable_resource_file_sharing =
      base::FeatureList::IsEnabled(features::kLacrosResourcesFileSharing);
  // If resource file sharing feature is disabled, clear the cached shared
  // resource file anyway.
  if (!params.enable_resource_file_sharing)
    clear_shared_resource_file = true;

  // Clear shared resource file cache if it's initial lacros launch after ash
  // reboot. If not, rename shared resource file cache to temporal name on
  // Lacros launch.
  if (ClearOrMoveSharedResourceFile(clear_shared_resource_file) ==
      ResourcesFileSharingMode::kError)
    params.enable_resource_file_sharing = false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kLacrosChromeAdditionalArgsFile)) {
    const base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            ash::switches::kLacrosChromeAdditionalArgsFile);
    std::string data;
    if (!base::ReadFileToString(path, &data)) {
      PLOG(WARNING) << "Unable to read from lacros additional args file "
                    << path.value();
    }
    std::vector<base::StringPiece> delimited_flags =
        base::SplitStringPieceUsingSubstr(data, "\n", base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_NONEMPTY);

    for (const auto& flag : delimited_flags) {
      if (flag[0] != '#')
        params.lacros_additional_args.emplace_back(flag);
    }
  }

  return params;
}

std::string GetXdgRuntimeDir() {
  // If ash-chrome was given an environment variable, use it.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_runtime_dir;
  if (env->GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir))
    return xdg_runtime_dir;

  // Otherwise provide the default for Chrome OS devices.
  return "/run/chrome";
}

void TerminateLacrosChrome(base::Process process, base::TimeDelta timeout) {
  // Here, lacros-chrome process may crashed, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this wait captures the process termination.
  if (process.WaitForExitWithTimeout(timeout, nullptr))
    return;

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
}

void SetLaunchOnLoginPref(bool launch_on_login) {
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      browser_util::kLaunchOnLoginPref, launch_on_login);
}

bool GetLaunchOnLoginPref() {
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      browser_util::kLaunchOnLoginPref);
}

bool IsKeepAliveDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kDisableLacrosKeepAliveForTesting);
}

bool IsLoginLacrosOpeningDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kDisableLoginLacrosOpening);
}

}  // namespace

// To be sure the lacros is running with neutral thread type.
class LacrosThreadTypeDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  void RunAsyncSafe() override {
    // TODO(crbug.com/1289736): Currently, this is causing some deadlock issue.
    // It looks like inside the function, we seem to call async unsafe API.
    // For the mitigation, disabling this temporarily.
    // We should revisit here, and see the impact of performance.
    // SetCurrentThreadType() needs file I/O on /proc and /sys.
    // base::ScopedAllowBlocking allow_blocking;
    // base::PlatformThread::SetCurrentThreadType(
    //     base::ThreadType::kDefault);
  }
};

// static
BrowserManager* BrowserManager::Get() {
  return g_instance;
}

BrowserManager::BrowserManager(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserManager(std::make_unique<BrowserLoader>(manager),
                     g_browser_process->component_updater()) {}

BrowserManager::BrowserManager(
    std::unique_ptr<BrowserLoader> browser_loader,
    component_updater::ComponentUpdateService* update_service)
    : browser_loader_(std::move(browser_loader)),
      component_update_service_(update_service),
      environment_provider_(std::make_unique<EnvironmentProvider>()),
      disabled_for_testing_(g_disabled_for_testing) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->AddObserver(this);

  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }

  std::string socket_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosMojoSocketForTesting);
  if (!socket_path.empty()) {
    test_mojo_connection_manager_ =
        std::make_unique<crosapi::TestMojoConnectionManager>(
            base::FilePath(socket_path), environment_provider_.get());
  }
}

BrowserManager::~BrowserManager() {
  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->RemoveObserver(this);
  }

  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);

  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*exit_code=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool BrowserManager::IsRunning() const {
  return state_ == State::RUNNING;
}

bool BrowserManager::IsRunningOrWillRun() const {
  return state_ == State::RUNNING || state_ == State::STARTING ||
         state_ == State::CREATING_LOG_FILE || state_ == State::TERMINATING;
}

void BrowserManager::NewWindow(bool incognito,
                               bool should_trigger_session_restore) {
  PerformOrEnqueue(
      BrowserAction::NewWindow(incognito, should_trigger_session_restore));
}

void BrowserManager::OpenForFullRestore(bool skip_crash_restore) {
  PerformOrEnqueue(BrowserAction::OpenForFullRestore(skip_crash_restore));
}

void BrowserManager::NewWindowForDetachingTab(
    const std::u16string& tab_id_str,
    const std::u16string& group_id_str,
    NewWindowForDetachingTabCallback callback) {
  PerformOrEnqueue(BrowserAction::NewWindowForDetachingTab(
      tab_id_str, group_id_str, std::move(callback)));
}

void BrowserManager::NewFullscreenWindow(const GURL& url,
                                         NewFullscreenWindowCallback callback) {
  PerformOrEnqueue(
      BrowserAction::NewFullscreenWindow(url, std::move(callback)));
}

void BrowserManager::NewGuestWindow() {
  PerformOrEnqueue(BrowserAction::NewGuestWindow());
}

void BrowserManager::NewTab(bool should_trigger_session_restore) {
  PerformOrEnqueue(BrowserAction::NewTab(should_trigger_session_restore));
}

void BrowserManager::Launch() {
  PerformOrEnqueue(BrowserAction::Launch());
}

void BrowserManager::OpenUrl(
    const GURL& url,
    crosapi::mojom::OpenUrlFrom from,
    crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition) {
  PerformOrEnqueue(
      BrowserAction::OpenUrl(url, disposition, from, NavigateParams::RESPECT));
}

void BrowserManager::SwitchToTab(const GURL& url,
                                 NavigateParams::PathBehavior path_behavior) {
  PerformOrEnqueue(BrowserAction::OpenUrl(
      url, crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab,
      crosapi::mojom::OpenUrlFrom::kUnspecified, path_behavior));
}

void BrowserManager::RestoreTab() {
  PerformOrEnqueue(BrowserAction::RestoreTab());
}

void BrowserManager::HandleTabScrubbing(float x_offset) {
  PerformOrEnqueue(BrowserAction::HandleTabScrubbing(x_offset));
}

void BrowserManager::CreateBrowserWithRestoredData(
    const std::vector<GURL>& urls,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    int32_t active_tab_index,
    const std::string& app_name,
    int32_t restore_window_id) {
  PerformOrEnqueue(BrowserAction::CreateBrowserWithRestoredData(
      urls, bounds, show_state, active_tab_index, app_name, restore_window_id));
}

void BrowserManager::InitializeAndStartIfNeeded() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  PrepareLacrosPolicies();

  // Perform the UMA recording for the current Lacros mode of operation.
  RecordLacrosLaunchMode();

  const bool is_lacros_enabled = browser_util::IsLacrosEnabled();

  // As a switch between Ash and Lacros mode requires an Ash restart plus
  // profile migration, the state will not change while the system is up.
  // At this point we are starting Lacros for the first time and with that the
  // operation mode is 'locked in'.
  crosapi::lacros_startup_state::SetLacrosStartupState(
      is_lacros_enabled, browser_util::IsLacrosPrimaryBrowser());

  // Must be checked after user session start because it depends on user type.
  if (is_lacros_enabled) {
    // Start Lacros automatically on login, if
    // 1) Lacros was opened in the previous session; or
    // 2) Lacros is the primary web browser.
    //    This can be suppressed via commandline flag for testing.
    if (GetLaunchOnLoginPref() || (browser_util::IsLacrosPrimaryBrowser() &&
                                   !IsLoginLacrosOpeningDisabledForTesting())) {
      pending_actions_.Push(BrowserAction::GetActionForSessionStart());
    }

    component_update_observation_.Observe(component_update_service_);
    SetState(State::MOUNTING);
    browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                         weak_factory_.GetWeakPtr()));
  } else {
    SetState(State::UNAVAILABLE);
    browser_loader_->Unload();
  }

  // Post `DryRunToCollectUMA()` to send UMA stats about sizes of files/dirs
  // inside the profile data directory.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ash::browser_data_migrator_util::DryRunToCollectUMA,
                     ProfileManager::GetPrimaryUserProfile()->GetPath()));
}

bool BrowserManager::GetFeedbackDataSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetFeedbackDataMinVersion;
}

// TODO(neis): Create BrowserAction also for this and others, perhaps even
// UpdateKeepAlive.
void BrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  DCHECK(GetFeedbackDataSupported());
  browser_service_->service->GetFeedbackData(std::move(callback));
}

bool BrowserManager::GetHistogramsSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetHistogramsMinVersion;
}

void BrowserManager::GetHistograms(GetHistogramsCallback callback) {
  DCHECK(GetHistogramsSupported());
  browser_service_->service->GetHistograms(std::move(callback));
}

bool BrowserManager::GetActiveTabUrlSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetActiveTabUrlMinVersion;
}

void BrowserManager::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  DCHECK(GetActiveTabUrlSupported());
  browser_service_->service->GetActiveTabUrl(std::move(callback));
}

void BrowserManager::GetTabStripModelUrls(
    const std::string& window_unique_id,
    GetTabStripModelUrlsCallback callback) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->desk_template_ash()
      ->GetTabStripModelUrls(window_unique_id, std::move(callback));
}

void BrowserManager::AddObserver(BrowserManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManager::RemoveObserver(BrowserManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserManager::Shutdown() {
  // Lacros KeepAlive should be disabled once Shutdown() has been signalled.
  // Further calls to `UpdateKeepAliveInBrowserIfNecessary()` will no-op after
  // `shutdown_requested_` has been set.
  UpdateKeepAliveInBrowserIfNecessary(false);
  shutdown_requested_ = true;
  pending_actions_.Clear();

  // The lacros-chrome process may have already been terminated as the result of
  // a previous mojo pipe disconnection in `OnMojoDisconnected()` and not yet
  // restarted. If, on the other hand, it is still valid, terminate it now.
  if (lacros_process_.IsValid()) {
    LOG(WARNING) << "Ash-chrome shutdown initiated. Terminating lacros-chrome";
    lacros_process_.Terminate(/*exit_code=*/0, /*wait=*/false);

    // Synchronously post a shutdown blocking task that waits for lacros-chrome
    // to cleanly exit. Terminate() will eventually result in a callback into
    // OnMojoDisconnected(), however this resolves asynchronously and there is a
    // risk that ash exits before this is called.
    // The 2.5s wait for a successful lacros exit stays below the 3s timeout
    // after which ash is forcefully terminated by the session_manager.
    HandleLacrosChromeTermination(base::Milliseconds(2500));
  }
}

void BrowserManager::set_relaunch_requested_for_testing(
    bool relaunch_requested) {
  CHECK_IS_TEST();
  relaunch_requested_ = relaunch_requested;
}

void BrowserManager::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;

  for (auto& observer : observers_) {
    if (state == State::TERMINATING) {
      observer.OnMojoDisconnected();
    }
    observer.OnStateChanged();
  }
}

BrowserManager::ScopedKeepAlive::~ScopedKeepAlive() {
  manager_->StopKeepAlive(feature_);
}

BrowserManager::ScopedKeepAlive::ScopedKeepAlive(BrowserManager* manager,
                                                 Feature feature)
    : manager_(manager), feature_(feature) {
  manager_->StartKeepAlive(feature_);
}

std::unique_ptr<BrowserManager::ScopedKeepAlive> BrowserManager::KeepAlive(
    Feature feature) {
  // Using new explicitly because ScopedKeepAlive's constructor is private.
  return base::WrapUnique(new ScopedKeepAlive(this, feature));
}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* service,
    uint32_t interface_version)
    : mojo_id(mojo_id),
      service(service),
      interface_version(interface_version) {}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    const BrowserServiceInfo&) = default;
BrowserManager::BrowserServiceInfo&
BrowserManager::BrowserServiceInfo::operator=(const BrowserServiceInfo&) =
    default;
BrowserManager::BrowserServiceInfo::~BrowserServiceInfo() = default;

void BrowserManager::Start() {
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!shutdown_requested_);
  DCHECK(!lacros_path_.empty());
  DCHECK(lacros_selection_.has_value());

  if (update_available_) {
    update_available_ = false;
    SetState(State::MOUNTING);
    lacros_path_ = base::FilePath();
    lacros_selection_ = absl::nullopt;
    // OnLoadComplete will call Start again.
    browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                         weak_factory_.GetWeakPtr()));
    return;
  }

  // Ensure we're not trying to open a window before the shelf is initialized.
  // Kiosk sessions don't need this check because they don't enable the shelf.
  DCHECK(user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp() ||
         ChromeShelfController::instance());

  // Always reset the |relaunch_requested_| flag when launching Lacros.
  relaunch_requested_ = false;

  SetState(State::CREATING_LOG_FILE);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DoLacrosBackgroundWorkPreLaunch, lacros_path_,
                     is_initial_lacros_launch_after_reboot_),
      base::BindOnce(&BrowserManager::StartWithLogFile,
                     weak_factory_.GetWeakPtr()));

  // Set false to prepare for the next Lacros launch.
  is_initial_lacros_launch_after_reboot_ = false;
}

void BrowserManager::StartWithLogFile(LaunchParamsFromBackground params) {
  DCHECK_EQ(state_, State::CREATING_LOG_FILE);

  // Shutdown() might have been called after Start() posted the StartWithLogFile
  // task, so we need to check `shutdown_requested_` again.
  if (shutdown_requested_) {
    LOG(ERROR) << "Start attempted after Shutdown() called.";
    SetState(State::STOPPED);
    return;
  }

  const std::string user_id_hash = ash::ProfileHelper::GetUserIdHashFromProfile(
      ProfileManager::GetPrimaryUserProfile());
  crosapi::browser_util::RecordDataVer(g_browser_process->local_state(),
                                       user_id_hash,
                                       version_info::GetVersion());

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  // If Ash is an unknown channel then this is not a production build and we
  // should be using an unknown channel for Lacros as well. This prevents Lacros
  // from picking up Finch experiments.
  version_info::Channel update_channel = version_info::Channel::UNKNOWN;
  if (chrome::GetChannel() != version_info::Channel::UNKNOWN) {
    DCHECK(lacros_selection_.has_value());
    update_channel = browser_util::GetLacrosSelectionUpdateChannel(
        lacros_selection_.value());
    // If we don't have channel information, we default to the "dev" channel.
    if (update_channel == version_info::Channel::UNKNOWN)
      update_channel = browser_util::kLacrosDefaultChannel;
  }

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();
  options.environment["CHROME_VERSION_EXTRA"] =
      version_info::GetChannelString(update_channel);

  std::string additional_env =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosChromeAdditionalEnv);
  base::StringPairs env_pairs;
  if (base::SplitStringIntoKeyValuePairsUsingSubstr(additional_env, '=', "####",
                                                    &env_pairs)) {
    for (const auto& env_pair : env_pairs) {
      if (!env_pair.first.empty()) {
        LOG(WARNING) << "Applying lacros env " << env_pair.first << "="
                     << env_pair.second;
        options.environment[env_pair.first] = env_pair.second;
      }
    }
  }

  options.kill_on_parent_death = true;

  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = browser_util::GetUserDataDir().AsUTF8Unsafe();
  std::string crash_dir =
      browser_util::GetUserDataDir().Append("Crash Reports").AsUTF8Unsafe();

  // Pass the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  std::string locale = g_browser_process->GetApplicationLocale();

  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--lang=" + locale,
                                   "--enable-webgl-image-chromium",
                                   "--breakpad-dump-location=" + crash_dir};

  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS())
    argv.push_back("--use-cras");

#if BUILDFLAG(ENABLE_NACL)
  // This switch is forwarded to nacl_helper and is needed before zygote fork.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerboseLoggingInNacl)) {
    argv.push_back("--verbose-logging-in-nacl=" +
                   base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                       switches::kVerboseLoggingInNacl));
  }
#endif

  std::string additional_flags =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosChromeAdditionalArgs);
  std::vector<base::StringPiece> delimited_flags =
      base::SplitStringPieceUsingSubstr(additional_flags, "####",
                                        base::TRIM_WHITESPACE,
                                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& flag : delimited_flags)
    argv.emplace_back(flag);

  argv.insert(argv.end(), params.lacros_additional_args.begin(),
              params.lacros_additional_args.end());

  // Forward flag for zero copy video capture to Lacros if it is enabled.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    argv.emplace_back(
        base::StringPrintf("--%s", switches::kVideoCaptureUseGpuMemoryBuffer));
  }

  // If logfd is valid, enable logging and redirect stdout/stderr to logfd.
  if (params.logfd.is_valid()) {
    // The next flag will make chrome log only via stderr. See
    // DetermineLoggingDestination in logging_chrome.cc.
    argv.push_back("--enable-logging=stderr");

    // These options will assign stdout/stderr fds to logfd in the fd table of
    // the new process.
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDOUT_FILENO));
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDERR_FILENO));
  }

  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      environment_provider_.get(),
      browser_util::InitialBrowserAction(
          mojom::InitialBrowserAction::kDoNotOpenWindow),
      !keep_alive_features_.empty(), lacros_selection_);
  if (startup_fd.is_valid()) {
    // Hardcoded to use FD 3 to make the ash-chrome's behavior more predictable.
    // Lacros-chrome should not depend on the hardcoded value though. Instead
    // it should take a look at the value passed via the command line flag.
    constexpr int kStartupDataFD = 3;
    argv.push_back(base::StringPrintf(
        "--%s=%d", chromeos::switches::kCrosStartupDataFD, kStartupDataFD));
    options.fds_to_remap.emplace_back(startup_fd.get(), kStartupDataFD);
  }

  // Set up Mojo channel.
  base::CommandLine command_line(argv);

  // Lacros-chrome starts with kNormal type
  LacrosThreadTypeDelegate thread_type_delegate;
  options.pre_exec_delegate = &thread_type_delegate;

  // Prepare to invite lacros-chrome to the Mojo universe of Crosapi.
  mojo::PlatformChannel channel;
  std::string channel_flag_value;
  channel.PrepareToPassRemoteEndpoint(&options.fds_to_remap,
                                      &channel_flag_value);
  DCHECK(!channel_flag_value.empty());
  command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                 channel_flag_value);
  DCHECK(!crosapi_id_.has_value());
  // Use new Crosapi mojo connection to detect process termination always.
  crosapi_id_ = CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(),
      base::BindOnce(&BrowserManager::OnMojoDisconnected,
                     weak_factory_.GetWeakPtr()));

  if (crash_reporter::IsCrashpadEnabled()) {
    command_line.AppendSwitch(switches::kEnableCrashpad);
  }

  if (params.enable_resource_file_sharing) {
    // Pass a flag to enable resources file sharing to Lacros.
    // To use resources file sharing feature on Lacros, it's required for ash to
    // run with enabling the feature as well since the feature is based on some
    // ash behavior(clear or move cached shared resource file at lacros launch).
    command_line.AppendSwitch(switches::kEnableResourcesFileSharing);
  }

  LOG(WARNING) << "Launching lacros with command: "
               << command_line.GetCommandLineString();

  // Create the lacros-chrome subprocess.
  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  lacros_launch_time_ = base::TimeTicks::Now();
  // If lacros_process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  lacros_process_ = base::LaunchProcess(command_line, options);
  if (!lacros_process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    // We give up, as this is most likely a permanent problem.
    SetState(State::UNAVAILABLE);
    return;
  }
  SetState(State::STARTING);
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
  channel.RemoteProcessLaunchAttempted();
}

void BrowserManager::OnBrowserServiceConnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* browser_service,
    uint32_t browser_service_version) {
  if (id != crosapi_id_) {
    // This BrowserService is unrelated to this instance. Skipping.
    return;
  }

  is_terminated_ = false;

  DCHECK(!browser_service_.has_value());
  browser_service_ =
      BrowserServiceInfo{mojo_id, browser_service, browser_service_version};
  base::UmaHistogramMediumTimes("ChromeOS.Lacros.StartTime",
                                base::TimeTicks::Now() - lacros_launch_time_);
  // Set the launch-on-login pref every time lacros-chrome successfully starts,
  // instead of once during ash-chrome shutdown, so we have the right value
  // even if ash-chrome crashes.
  SetLaunchOnLoginPref(true);
  LOG(WARNING) << "Connection to lacros-chrome is established.";

  DCHECK_EQ(state_, State::STARTING);
  SetState(State::RUNNING);

  // There can be a chance that keep_alive status is updated between the
  // process launching timing (where initial_keep_alive is set) and the
  // crosapi mojo connection timing (i.e., this function).
  // So, send it to lacros-chrome to update to fill the possible gap.
  UpdateKeepAliveInBrowserIfNecessary(!keep_alive_features_.empty());

  while (!pending_actions_.IsEmpty()) {
    pending_actions_.Pop()->Perform(
        {browser_service_.value().service,
         browser_service_.value().interface_version});
    DCHECK_EQ(state_, State::RUNNING);
  }
}

void BrowserManager::OnBrowserServiceDisconnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id) {
  // No need to check CrosapiId here, because |mojo_id| is unique within
  // a process.
  if (browser_service_.has_value() && browser_service_->mojo_id == mojo_id)
    browser_service_.reset();
}

void BrowserManager::OnBrowserRelaunchRequested(CrosapiId id) {
  if (id != crosapi_id_)
    return;
  relaunch_requested_ = true;
}

void BrowserManager::OnCoreConnected(policy::CloudPolicyCore* core) {}

void BrowserManager::OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) {
  core->refresh_scheduler()->AddObserver(this);
}

void BrowserManager::OnCoreDisconnecting(policy::CloudPolicyCore* core) {}

void BrowserManager::OnCoreDestruction(policy::CloudPolicyCore* core) {
  core->RemoveObserver(this);
}

void BrowserManager::OnMojoDisconnected() {
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";
  HandleLacrosChromeTermination(base::Seconds(5));
}

void BrowserManager::HandleLacrosChromeTermination(base::TimeDelta timeout) {
  // This may be called following a synchronous termination in `Shutdown()` or
  // when the mojo pipe with the lacros-chrome process has disconnected. Early
  // return if already handling lacros-chrome termination.
  if (!lacros_process_.IsValid())
    return;

  DCHECK(state_ == State::STARTING || state_ == State::RUNNING);
  DCHECK(lacros_process_.IsValid());

  browser_service_.reset();
  crosapi_id_.reset();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&TerminateLacrosChrome, std::move(lacros_process_),
                     timeout),
      base::BindOnce(&BrowserManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()));

  SetState(State::TERMINATING);
}

void BrowserManager::OnLacrosChromeTerminated() {
  DCHECK_EQ(state_, State::TERMINATING);
  LOG(WARNING) << "Lacros-chrome is terminated";
  is_terminated_ = true;
  SetState(State::STOPPED);

  // TODO(https://crbug.com/1109366): Restart lacros-chrome if it exits
  // abnormally (e.g. crashes). For now, assume the user meant to close it.
  // Relaunch lacros-chrome if it was closed due to ash shutting down.
  // Note that this only matters for side-by-side lacros.
  SetLaunchOnLoginPref(shutdown_requested_);

  if (relaunch_requested_)
    pending_actions_.Push(
        BrowserAction::OpenForFullRestore(/*skip_crash_restore=*/true));
  StartIfNeeded();
}

void BrowserManager::OnSessionStateChanged() {
  if (disabled_for_testing_) {
    CHECK_IS_TEST();
    LOG(WARNING)
        << "BrowserManager disabled for testing, entering UNAVAILABLE state";
    SetState(State::UNAVAILABLE);
    return;
  }

  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() !=
      session_manager::SessionState::ACTIVE) {
    LOG(WARNING)
        << "Session not yet active. Lacros-chrome will not be launched yet";
    return;
  }

  // Launch Lacros if appropriate.
  if (browser_util::IsLacrosEnabled()) {
    if (!browser_util::IsLacrosAllowedToLaunch()) {
      std::unique_ptr<message_center::Notification> notification =
          ash::CreateSystemNotification(
              message_center::NOTIFICATION_TYPE_SIMPLE,
              kLacrosCannotLaunchNotificationID,
              /*title=*/std::u16string(),
              l10n_util::GetStringUTF16(
                  IDS_LACROS_CANNOT_LAUNCH_MULTI_SIGNIN_MESSAGE),
              /* display_source= */ std::u16string(), GURL(),
              message_center::NotifierId(
                  message_center::NotifierType::SYSTEM_COMPONENT,
                  kLacrosLauncherNotifierID,
                  ash::NotificationCatalogName::kLacrosCannotLaunch),
              message_center::RichNotificationData(),
              base::MakeRefCounted<
                  message_center::HandleNotificationClickDelegate>(
                  base::RepeatingClosure()),
              gfx::kNoneIcon,
              message_center::SystemNotificationWarningLevel::NORMAL);
      SystemNotificationHelper::GetInstance()->Display(*notification);
    } else {
      InitializeAndStartIfNeeded();
    }
  }

  // If "Go to files" on the migration error page was clicked, launch it here.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  std::string user_id_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile);
  if (browser_util::WasGotoFilesClicked(g_browser_process->local_state(),
                                        user_id_hash)) {
    files_app_launcher_ = std::make_unique<FilesAppLauncher>(
        apps::AppServiceProxyFactory::GetForProfile(profile));
    files_app_launcher_->Launch(base::BindOnce(
        browser_util::ClearGotoFilesClicked, g_browser_process->local_state(),
        std::move(user_id_hash)));
  }
}

void BrowserManager::OnStoreLoaded(policy::CloudPolicyStore* store) {
  DCHECK(store);

  // A new policy got installed for the current user, so we need to pass it to
  // the Lacros browser.
  std::string policy_blob;
  if (store->policy_fetch_response()) {
    const bool success =
        store->policy_fetch_response()->SerializeToString(&policy_blob);
    DCHECK(success);
  }
  SetDeviceAccountPolicy(policy_blob);
}

void BrowserManager::OnStoreError(policy::CloudPolicyStore* store) {
  // Policy store failed, Lacros will use stale policy as well as Ash.
}

void BrowserManager::OnStoreDestruction(policy::CloudPolicyStore* store) {
  store->RemoveObserver(this);
}

void BrowserManager::OnComponentPolicyUpdated(
    const policy::ComponentPolicyMap& component_policy) {
  environment_provider_->SetDeviceAccountComponentPolicy(
      policy::CopyComponentPolicyMap(component_policy));
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateComponentPolicy(
        policy::CopyComponentPolicyMap(component_policy));
  }
}

void BrowserManager::OnComponentPolicyServiceDestruction(
    policy::ComponentCloudPolicyService* service) {
  service->RemoveObserver(this);
}

void BrowserManager::OnFetchAttempt(
    policy::CloudPolicyRefreshScheduler* scheduler) {
  environment_provider_->SetLastPolicyFetchAttemptTimestamp(
      scheduler->last_refresh());
  if (browser_service_.has_value())
    browser_service_->service->NotifyPolicyFetchAttempt();
}

void BrowserManager::OnRefreshSchedulerDestruction(
    policy::CloudPolicyRefreshScheduler* scheduler) {
  scheduler->RemoveObserver(this);
}

void BrowserManager::OnEvent(Events event, const std::string& id) {
  // Track whether an update has been installed and should be loaded next time
  // the browser is started.
  if (event == Events::COMPONENT_UPDATED &&
      id == browser_util::GetLacrosComponentInfo().crx_id) {
    update_available_ = true;
  }
}

void BrowserManager::OnLoadComplete(const base::FilePath& path,
                                    LacrosSelection selection) {
  if (shutdown_requested_) {
    LOG(ERROR) << "Load completed after Shutdown() called.";
    return;
  }
  DCHECK_EQ(state_, State::MOUNTING);

  lacros_path_ = path;
  lacros_selection_ = absl::optional<LacrosSelection>(selection);
  const bool success = !path.empty();
  SetState(success ? State::STOPPED : State::UNAVAILABLE);
  // TODO(crbug.com/1266010): In the event the load operation failed, we should
  // launch the last successfully loaded image.
  for (auto& observer : observers_) {
    observer.OnLoadComplete(success);
  }

  StartIfNeeded();
}

void BrowserManager::StartIfNeeded() {
  if (state_ == State::STOPPED && !shutdown_requested_)
    if (!pending_actions_.IsEmpty() || IsKeepAliveEnabled())
      Start();
}

void BrowserManager::PrepareLacrosPolicies() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  policy::CloudPolicyCore* core = nullptr;
  policy::ComponentCloudPolicyService* component_policy_service = nullptr;
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD: {
      Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
      DCHECK(profile);
      policy::CloudPolicyManager* user_cloud_policy_manager =
          profile->GetUserCloudPolicyManagerAsh();
      if (user_cloud_policy_manager) {
        core = user_cloud_policy_manager->core();
        component_policy_service =
            user_cloud_policy_manager->component_policy_service();
      }
      break;
    }
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_WEB_KIOSK_APP: {
      policy::DeviceLocalAccountPolicyBroker* broker =
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->GetDeviceLocalAccountPolicyService()
              ->GetBrokerForUser(user->GetAccountId().GetUserEmail());
      if (broker) {
        core = broker->core();
        component_policy_service = broker->component_policy_service();
      }
      break;
    }
    default:
      break;
  }

  // The lifetime of `BrowserManager` is longer than lifetime of various
  // classes, for which we register as an observer below. The RemoveObserver
  // function is therefore called in various handlers invoked by those classes
  // and not in the destructor.
  if (core) {
    core->AddObserver(this);
    if (core->refresh_scheduler())
      core->refresh_scheduler()->AddObserver(this);

    policy::CloudPolicyStore* store = core->store();
    if (store && store->policy_fetch_response()) {
      const std::string policy_blob =
          store->policy_fetch_response()->SerializeAsString();
      SetDeviceAccountPolicy(policy_blob);
      store->AddObserver(this);
    }
  }

  if (component_policy_service)
    component_policy_service->AddObserver(this);
}

void BrowserManager::SetDeviceAccountPolicy(const std::string& policy_blob) {
  environment_provider_->SetDeviceAccountPolicy(policy_blob);
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateDeviceAccountPolicy(
        std::vector<uint8_t>(policy_blob.begin(), policy_blob.end()));
  }
}

LaunchParamsFromBackground::LaunchParamsFromBackground() = default;
LaunchParamsFromBackground::LaunchParamsFromBackground(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground::~LaunchParamsFromBackground() = default;

void BrowserManager::StartKeepAlive(Feature feature) {
  DCHECK(browser_util::IsLacrosEnabled());

  if (IsKeepAliveDisabledForTesting())
    return;

  DCHECK(keep_alive_features_.find(feature) == keep_alive_features_.end())
      << "Features should never be double registered.";

  keep_alive_features_.insert(feature);
  // If this is first KeepAlive instance, update the keep-alive in the browser.
  if (keep_alive_features_.size() == 1)
    UpdateKeepAliveInBrowserIfNecessary(true);
  StartIfNeeded();
}

void BrowserManager::StopKeepAlive(Feature feature) {
  keep_alive_features_.erase(feature);
  if (!IsKeepAliveEnabled())
    UpdateKeepAliveInBrowserIfNecessary(false);
}

bool BrowserManager::IsKeepAliveEnabled() const {
  return !keep_alive_features_.empty();
}

void BrowserManager::UpdateKeepAliveInBrowserIfNecessary(bool enabled) {
  if (shutdown_requested_ || !browser_service_.has_value() ||
      browser_service_->interface_version <
          crosapi::mojom::BrowserService::kUpdateKeepAliveMinVersion) {
    // Shutdown has started, the browser is not running now, or Lacros is too
    // old. Just give up.
    return;
  }
  browser_service_->service->UpdateKeepAlive(enabled);
}

void BrowserManager::RecordLacrosLaunchMode() {
  LacrosLaunchMode lacros_mode;
  LacrosLaunchModeAndSource lacros_mode_and_source;

  if (!browser_util::IsAshWebBrowserEnabled()) {
    // As Ash is disabled, Lacros is the only available browser.
    lacros_mode = LacrosLaunchMode::kLacrosOnly;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserLacrosOnly;
  } else if (browser_util::IsLacrosPrimaryBrowser()) {
    // Lacros is the primary browser - but Ash is still available.
    lacros_mode = LacrosLaunchMode::kLacrosPrimary;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserLacrosPrimary;
  } else if (browser_util::IsLacrosEnabled()) {
    // If Lacros is enabled but not primary or the only browser, the
    // side by side mode is active.
    lacros_mode = LacrosLaunchMode::kSideBySide;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserSideBySide;

  } else {
    lacros_mode = LacrosLaunchMode::kLacrosDisabled;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserLacrosDisabled;
  }

  UMA_HISTOGRAM_ENUMERATION("Ash.Lacros.Launch.Mode", lacros_mode);

  crosapi::browser_util::LacrosLaunchSwitchSource source =
      crosapi::browser_util::GetLacrosLaunchSwitchSource();

  // Make sure we have always the policy loaded before we get here.
  DCHECK(source != crosapi::browser_util::LacrosLaunchSwitchSource::kUnknown);

  LacrosLaunchModeAndSource source_offset;
  if (source ==
      crosapi::browser_util::LacrosLaunchSwitchSource::kPossiblySetByUser) {
    source_offset = LacrosLaunchModeAndSource::kPossiblySetByUserLacrosDisabled;
  } else if (source ==
             crosapi::browser_util::LacrosLaunchSwitchSource::kForcedByUser) {
    source_offset = LacrosLaunchModeAndSource::kForcedByUserLacrosDisabled;
  } else {
    source_offset = LacrosLaunchModeAndSource::kForcedByPolicyLacrosDisabled;
  }

  // The states are comprised of the basic four Lacros options and the
  // source of the mode selection (By user, by Policy, by System). These
  // combinations are "nibbled together" here in one status value.
  lacros_mode_and_source = static_cast<LacrosLaunchModeAndSource>(
      static_cast<int>(source_offset) +
      static_cast<int>(lacros_mode_and_source));

  UMA_HISTOGRAM_ENUMERATION("Ash.Lacros.Launch.ModeAndSource",
                            lacros_mode_and_source);
  if (!lacros_mode_.has_value() || !lacros_mode_and_source_.has_value() ||
      lacros_mode != *lacros_mode_ ||
      lacros_mode_and_source != *lacros_mode_and_source_) {
    // Remember new values.
    lacros_mode_ = lacros_mode;
    lacros_mode_and_source_ = lacros_mode_and_source;

    // Call our Daily launch mode reporting once now to make sure we have an
    // event. If it's a dupe, the server will de-dupe.
    OnDailyLaunchModeTimer();
    if (!daily_event_timer_.IsRunning()) {
      daily_event_timer_.Start(FROM_HERE, kDailyLaunchModeTimeDelta, this,
                               &BrowserManager::OnDailyLaunchModeTimer);
    }
  }
}

void BrowserManager::PerformOrEnqueue(std::unique_ptr<BrowserAction> action) {
  if (shutdown_requested_) {
    LOG(WARNING) << "lacros-chrome is preparing for system shutdown";
    action->Cancel(mojom::CreationResult::kBrowserNotRunning);
    return;
  }

  switch (state_) {
    case State::UNAVAILABLE:
      LOG(ERROR) << "lacros unavailable";
      action->Cancel(mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::NOT_INITIALIZED:
    case State::MOUNTING:
      LOG(WARNING) << "lacros component image not yet available";
      pending_actions_.PushOrCancel(std::move(action));
      return;
    case State::TERMINATING:
      LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
      pending_actions_.PushOrCancel(std::move(action));
      return;
    case State::CREATING_LOG_FILE:
    case State::STARTING:
      LOG(WARNING) << "lacros-chrome is in the process of launching";
      pending_actions_.PushOrCancel(std::move(action));
      return;

    case State::STOPPED:
      DCHECK(!IsKeepAliveEnabled());
      DCHECK(pending_actions_.IsEmpty());
      pending_actions_.PushOrCancel(std::move(action));
      StartIfNeeded();
      return;

    case State::RUNNING:
      if (!browser_service_.has_value()) {
        LOG(ERROR) << "BrowserService was disconnected";
        action->Cancel(mojom::CreationResult::kServiceDisconnected);
        return;
      }
      action->Perform(
          {browser_service_->service, browser_service_->interface_version});
      return;
  }
}

// Callback called when the daily event happens.
void BrowserManager::OnDailyLaunchModeTimer() {
  UMA_HISTOGRAM_ENUMERATION(kLacrosLaunchModeDaily, *lacros_mode_);
  UMA_HISTOGRAM_ENUMERATION(kLacrosLaunchModeAndSourceDaily,
                            *lacros_mode_and_source_);
}

// static
void BrowserManager::DisableForTesting() {
  CHECK_IS_TEST();
  g_disabled_for_testing = true;
}

}  // namespace crosapi
