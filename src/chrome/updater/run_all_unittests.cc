// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/unittest_util.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "chrome/updater/win/win_util.h"

namespace {

// If the priority class is not NORMAL_PRIORITY_CLASS, then the function makes:
// - the priority class of the process NORMAL_PRIORITY_CLASS
// - the process memory priority MEMORY_PRIORITY_NORMAL
// - the current thread priority THREAD_PRIORITY_NORMAL
void FixExecutionPriorities() {
  const HANDLE process = ::GetCurrentProcess();
  const DWORD priority_class = ::GetPriorityClass(process);
  if (priority_class == NORMAL_PRIORITY_CLASS)
    return;
  ::SetPriorityClass(process, NORMAL_PRIORITY_CLASS);

  static const auto set_process_information_fn =
      reinterpret_cast<decltype(&::SetProcessInformation)>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "SetProcessInformation"));
  if (!set_process_information_fn)
    return;
  MEMORY_PRIORITY_INFORMATION memory_priority = {};
  memory_priority.MemoryPriority = MEMORY_PRIORITY_NORMAL;
  set_process_information_fn(process, ProcessMemoryPriority, &memory_priority,
                             sizeof(memory_priority));

  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

// Sets the _NT_ALT_SYMBOL_PATH for the system or the user, if it is not set
// already. Resets it on destruction. The environment variable is set in the
// corresponding registry hive. _NT_ALT_SYMBOL_PATH is used to avoid symbol
// path collision because its usage is less common than _NT_SYMBOL_PATH. The
// environment variable points to the directory where this unit test binary is.
// The symbol files for the updater targets are expected to be present in this
// directory.
class ScopedSymbolPath {
 public:
  explicit ScopedSymbolPath(bool is_system)
      : is_system_(is_system),
        rootkey_(is_system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER),
        subkey_(is_system ? L"SYSTEM\\CurrentControlSet\\Control\\Session "
                            L"Manager\\Environment"
                          : L"Environment") {
    base::FilePath out_dir;
    base::PathService::Get(base::DIR_EXE, &out_dir);
    const std::wstring symbol_path = out_dir.value();

    // For an unknown reason, symbolized stacks for code running as user
    // requires setting up the environment variable for this unit test process.
    if (::GetEnvironmentVariable(kNtSymbolPathEnVar, nullptr, 0) == 0) {
      ::SetEnvironmentVariable(kNtSymbolPathEnVar, symbol_path.c_str());
    }

    base::win::RegKey reg_key(rootkey_, subkey_.c_str(), KEY_READ | KEY_WRITE);
    if (reg_key.Valid() && !reg_key.HasValue(kNtSymbolPathEnVar)) {
      is_owned = reg_key.WriteValue(kNtSymbolPathEnVar, symbol_path.c_str()) ==
                 ERROR_SUCCESS;
      if (!is_owned)
        return;
      BroadcastEnvironmentChange();
      std::wcerr << "Symbol path for " << (is_system_ ? "system" : "user")
                 << " set to: " << symbol_path << std::endl;
    }
  }

  ~ScopedSymbolPath() {
    if (!is_owned)
      return;
    base::win::RegKey reg_key(rootkey_, subkey_.c_str(), KEY_WRITE);
    if (reg_key.Valid()) {
      reg_key.DeleteValue(kNtSymbolPathEnVar);
      BroadcastEnvironmentChange();
    }
  }

 private:
  // Notifies the processes that the environment has been changed to reload it.
  static void BroadcastEnvironmentChange() {
    constexpr int kTimeOutMilliSeconds = 100;
    DWORD_PTR result = 0;
    ::SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                         reinterpret_cast<LPARAM>(L"Environment"), SMTO_NORMAL,
                         kTimeOutMilliSeconds, &result);
  }

  // The name of the environment variable to create under the registry key.
  static constexpr wchar_t kNtSymbolPathEnVar[] = L"_NT_ALT_SYMBOL_PATH";

  const bool is_system_ = false;
  const HKEY rootkey_ = nullptr;
  const std::wstring subkey_;

  // True if the registry value is owned by this instance and it must be
  // cleaned up on destruction.
  bool is_owned = false;
};

}  // namespace

#endif  // BUILDFLAG(IS_WIN)

namespace {

void MaybeIncreaseTestTimeouts(int argc, char** argv) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // The minimum and the default value when unspecified is 45000.
  if (!command_line->HasSwitch(switches::kTestLauncherTimeout)) {
    command_line->AppendSwitchASCII(switches::kTestLauncherTimeout, "90000");
  }

  // The minimum and the default value when unspecified is 30000.
  if (!command_line->HasSwitch(switches::kUiTestActionMaxTimeout)) {
    command_line->AppendSwitchASCII(switches::kUiTestActionMaxTimeout, "45000");
  }

  // The minimum and the default value when unspecified is 10000.
  if (!command_line->HasSwitch(switches::kUiTestActionTimeout)) {
    command_line->AppendSwitchASCII(switches::kUiTestActionTimeout, "40000");
  }
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::ScopedClosureRunner reset_command_line(
      base::BindOnce(&base::CommandLine::Reset));

  // Change the test timeout defaults if the command line arguments to override
  // them are not present.
  MaybeIncreaseTestTimeouts(argc, argv);

#if BUILDFLAG(IS_WIN)
  std::cerr << "Process priority: " << base::Process::Current().GetPriority()
            << std::endl;
  std::cerr << updater::GetUACState() << std::endl;

  // TODO(crbug.com/1245429): remove when the bug is fixed.
  // Typically, the test suite runner expects the swarming task to run with
  // normal priority but for some reason, on the updater bots with UAC on, the
  // swarming task runs with a priority below normal.
  FixExecutionPriorities();

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
  if (FAILED(updater::DisableCOMExceptionHandling())) {
    // Failing to disable COM exception handling is a critical error.
    CHECK(false) << "Failed to disable COM exception handling.";
  }

  installer::ScopedTokenPrivilege token_se_debug(SE_DEBUG_NAME);
  if (::IsUserAnAdmin() && !token_se_debug.is_enabled()) {
    std::cerr << "Running as administrator but can't enable SE_DEBUG_NAME."
              << std::endl;
  }

  // Set up the _NT_ALT_SYMBOL_PATH to get symbolized stack traces in logs.
  ScopedSymbolPath scoped_symbol_path_system(/*is_system=*/true);
  ScopedSymbolPath scoped_symbol_path_user(/*is_system=*/false);
#endif

  base::TestSuite test_suite(argc, argv);
  chrome::RegisterPathProvider();
  return base::LaunchUnitTestsWithOptions(
      argc, argv, 1, 10, true, base::BindRepeating([]() {
        logging::SetLogItems(true,    // enable_process_id
                             true,    // enable_thread_id
                             true,    // enable_timestamp
                             false);  // enable_tickcount
        LOG(ERROR) << "A test timeout has occured in "
                   << updater::test::GetTestName();
#if BUILDFLAG(IS_WIN)
        const base::FilePath updater_test = updater::test::GetUpdaterTestPath();
        PLOG_IF(0, !base::PathExists(updater_test)) << ", " << updater_test;
#endif
        updater::test::CreateIntegrationTestCommands()->PrintLog();
      }),
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
