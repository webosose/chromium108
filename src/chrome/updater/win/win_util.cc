// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_util.h"

#include <aclapi.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>
#include <wtsapi32.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/scoped_variant.h"
#include "base/win/startup_information.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

HResultOr<bool> IsUserRunningSplitToken() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return base::unexpected(HRESULTFromLastError());
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(token_holder.Get(), TokenElevationType,
                             &elevation_type, sizeof(elevation_type),
                             &size_returned)) {
    return base::unexpected(HRESULTFromLastError());
  }
  bool is_split_token = elevation_type == TokenElevationTypeFull ||
                        elevation_type == TokenElevationTypeLimited;
  DCHECK(is_split_token || elevation_type == TokenElevationTypeDefault);
  return base::ok(is_split_token);
}

HRESULT GetSidIntegrityLevel(PSID sid, MANDATORY_LEVEL* level) {
  if (!::IsValidSid(sid))
    return E_FAIL;
  SID_IDENTIFIER_AUTHORITY* authority = ::GetSidIdentifierAuthority(sid);
  if (!authority)
    return E_FAIL;
  constexpr SID_IDENTIFIER_AUTHORITY kMandatoryLabelAuth =
      SECURITY_MANDATORY_LABEL_AUTHORITY;
  if (std::memcmp(authority, &kMandatoryLabelAuth,
                  sizeof(SID_IDENTIFIER_AUTHORITY))) {
    return E_FAIL;
  }
  PUCHAR count = ::GetSidSubAuthorityCount(sid);
  if (!count || *count != 1)
    return E_FAIL;
  DWORD* rid = ::GetSidSubAuthority(sid, 0);
  if (!rid)
    return E_FAIL;
  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    return E_FAIL;
  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Gets the mandatory integrity level of a process.
// TODO(crbug.com/1233748): consider reusing
// base::GetCurrentProcessIntegrityLevel().
HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, process_id);
  if (!process)
    return HRESULTFromLastError();
  base::win::ScopedHandle process_holder(process);
  HANDLE token = NULL;
  if (!::OpenProcessToken(process_holder.Get(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE, &token)) {
    return HRESULTFromLastError();
  }
  base::win::ScopedHandle token_holder(token);
  DWORD label_size = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel, nullptr, 0,
                            &label_size) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return E_FAIL;
  }
  std::unique_ptr<TOKEN_MANDATORY_LABEL, base::FreeDeleter> label(
      static_cast<TOKEN_MANDATORY_LABEL*>(std::malloc(label_size)));
  if (!::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel,
                             label.get(), label_size, &label_size)) {
    return HRESULTFromLastError();
  }
  return GetSidIntegrityLevel(label->Label.Sid, level);
}

bool IsExplorerRunningAtMediumOrLower() {
  base::NamedProcessIterator iter(L"EXPLORER.EXE", nullptr);
  while (const base::ProcessEntry* process_entry = iter.NextProcessEntry()) {
    MANDATORY_LEVEL level = MandatoryLevelUntrusted;
    if (SUCCEEDED(GetProcessIntegrityLevel(process_entry->pid(), &level)) &&
        level <= MandatoryLevelMedium) {
      return true;
    }
  }
  return false;
}

// Creates a WS_POPUP | WS_VISIBLE with zero
// size, of the STATIC WNDCLASS. It uses the default running EXE module
// handle for creation.
//
// A visible centered foreground window is needed as the parent in Windows 7 and
// above, to allow the UAC prompt to come up in the foreground, centered.
// Otherwise, the elevation prompt will be minimized on the taskbar. A zero size
// window works. A plain vanilla WS_POPUP allows the window to be free of
// adornments. WS_EX_TOOLWINDOW prevents the task bar from showing the
// zero-sized window.
//
// Returns NULL on failure. Call ::GetLastError() to get extended error
// information on failure.
HWND CreateForegroundParentWindowForUAC() {
  CWindow foreground_parent;
  if (foreground_parent.Create(L"STATIC", NULL, NULL, NULL,
                               WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW)) {
    foreground_parent.CenterWindow(NULL);
    ::SetForegroundWindow(foreground_parent);
  }
  return foreground_parent.Detach();
}

// Compares the OS, service pack, and build numbers using `::VerifyVersionInfo`,
// in accordance with `type_mask` and `oper`.
bool CompareOSVersionsInternal(const OSVERSIONINFOEX& os,
                               DWORD type_mask,
                               BYTE oper) {
  DCHECK(type_mask);
  DCHECK(oper);

  ULONGLONG cond_mask = 0;
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MAJORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MINORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMINOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_BUILDNUMBER, oper);

  // `::VerifyVersionInfo` could return `FALSE` due to an error other than
  // `ERROR_OLD_WIN_VERSION`. We do not handle that case here.
  // https://msdn.microsoft.com/ms725492.
  OSVERSIONINFOEX os_in = os;
  return ::VerifyVersionInfo(&os_in, type_mask, cond_mask);
}

}  // namespace

NamedObjectAttributes::NamedObjectAttributes(const std::wstring& name,
                                             const CSecurityDesc& sd)
    : name(name), sa(CSecurityAttributes(sd)) {}
NamedObjectAttributes::~NamedObjectAttributes() = default;

HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

// This sets up COM security to allow NetworkService, LocalService, and System
// to call back into the process. It is largely inspired by
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378987.aspx
// static
bool InitializeCOMSecurity() {
  // Create the security descriptor explicitly as follows because
  // CoInitializeSecurity() will not accept the relative security descriptors
  // returned by ConvertStringSecurityDescriptorToSecurityDescriptor().
  const size_t kSidCount = 5;
  uint64_t* sids[kSidCount][(SECURITY_MAX_SID_SIZE + sizeof(uint64_t) - 1) /
                            sizeof(uint64_t)] = {
      {}, {}, {}, {}, {},
  };

  // These are ordered by most interesting ones to try first.
  WELL_KNOWN_SID_TYPE sid_types[kSidCount] = {
      WinBuiltinAdministratorsSid,  // administrator group security identifier
      WinLocalServiceSid,           // local service security identifier
      WinNetworkServiceSid,         // network service security identifier
      WinSelfSid,                   // personal account security identifier
      WinLocalSystemSid,            // local system security identifier
  };

  // This creates a security descriptor that is equivalent to the following
  // security descriptor definition language (SDDL) string:
  //   O:BAG:BAD:(A;;0x1;;;LS)(A;;0x1;;;NS)(A;;0x1;;;PS)
  //   (A;;0x1;;;SY)(A;;0x1;;;BA)

  // Initialize the security descriptor.
  SECURITY_DESCRIPTOR security_desc = {};
  if (!::InitializeSecurityDescriptor(&security_desc,
                                      SECURITY_DESCRIPTOR_REVISION))
    return false;

  DCHECK_EQ(kSidCount, std::size(sids));
  DCHECK_EQ(kSidCount, std::size(sid_types));
  for (size_t i = 0; i < kSidCount; ++i) {
    DWORD sid_bytes = sizeof(sids[i]);
    if (!::CreateWellKnownSid(sid_types[i], nullptr, sids[i], &sid_bytes))
      return false;
  }

  // Setup the access control entries (ACE) for COM. You may need to modify
  // the access permissions for your application. COM_RIGHTS_EXECUTE and
  // COM_RIGHTS_EXECUTE_LOCAL are the minimum access rights required.
  EXPLICIT_ACCESS explicit_access[kSidCount] = {};
  DCHECK_EQ(kSidCount, std::size(sids));
  DCHECK_EQ(kSidCount, std::size(explicit_access));
  for (size_t i = 0; i < kSidCount; ++i) {
    explicit_access[i].grfAccessPermissions =
        COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
    explicit_access[i].grfAccessMode = SET_ACCESS;
    explicit_access[i].grfInheritance = NO_INHERITANCE;
    explicit_access[i].Trustee.pMultipleTrustee = nullptr;
    explicit_access[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicit_access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicit_access[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicit_access[i].Trustee.ptstrName = reinterpret_cast<LPTSTR>(sids[i]);
  }

  // Create an access control list (ACL) using this ACE list, if this succeeds
  // make sure to ::LocalFree(acl).
  ACL* acl = nullptr;
  DWORD acl_result = ::SetEntriesInAcl(std::size(explicit_access),
                                       explicit_access, nullptr, &acl);
  if (acl_result != ERROR_SUCCESS || acl == nullptr)
    return false;

  HRESULT hr = E_FAIL;

  // Set the security descriptor owner and group to Administrators and set the
  // discretionary access control list (DACL) to the ACL.
  if (::SetSecurityDescriptorOwner(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorGroup(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorDacl(&security_desc, TRUE, acl, FALSE)) {
    // Initialize COM. You may need to modify the parameters of
    // CoInitializeSecurity() for your application. Note that an
    // explicit security descriptor is being passed down.
    hr = ::CoInitializeSecurity(
        &security_desc, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
        EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL, nullptr);
  }

  ::LocalFree(acl);
  return SUCCEEDED(hr);
}

HMODULE GetModuleHandleFromAddress(void* address) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  DCHECK_EQ(result, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}

HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(
      reinterpret_cast<void*>(&GetCurrentModuleHandle));
}

// The event name saved to the environment variable does not contain the
// decoration added by GetNamedObjectAttributes.
HRESULT CreateUniqueEventInEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  const std::wstring event_name = base::ASCIIToWide(base::GenerateGUID());
  NamedObjectAttributes attr =
      GetNamedObjectAttributes(event_name.c_str(), scope);

  HRESULT hr = CreateEvent(&attr, unique_event);
  if (FAILED(hr))
    return hr;

  if (!::SetEnvironmentVariable(var_name.c_str(), event_name.c_str()))
    return HRESULTFromLastError();

  return S_OK;
}

HRESULT OpenUniqueEventFromEnvironment(const std::wstring& var_name,
                                       UpdaterScope scope,
                                       HANDLE* unique_event) {
  DCHECK(unique_event);

  wchar_t event_name[MAX_PATH] = {0};
  if (!::GetEnvironmentVariable(var_name.c_str(), event_name,
                                std::size(event_name))) {
    return HRESULTFromLastError();
  }

  NamedObjectAttributes attr = GetNamedObjectAttributes(event_name, scope);
  *unique_event = ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name.c_str());

  if (!*unique_event)
    return HRESULTFromLastError();

  return S_OK;
}

HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle) {
  DCHECK(event_handle);
  DCHECK(event_attr);
  DCHECK(!event_attr->name.empty());
  *event_handle = ::CreateEvent(&event_attr->sa,
                                true,   // manual reset
                                false,  // not signaled
                                event_attr->name.c_str());

  if (!*event_handle)
    return HRESULTFromLastError();

  return S_OK;
}

NamedObjectAttributes GetNamedObjectAttributes(const wchar_t* base_name,
                                               UpdaterScope scope) {
  DCHECK(base_name);

  switch (scope) {
    case UpdaterScope::kUser: {
      std::wstring user_sid;
      GetProcessUser(nullptr, nullptr, &user_sid);
      return {
          base::StrCat({kGlobalPrefix, base_name, user_sid}),
          GetCurrentUserDefaultSecurityDescriptor().value_or(CSecurityDesc())};
    }
    case UpdaterScope::kSystem:
      // Grant access to administrators and system.
      return {base::StrCat({kGlobalPrefix, base_name}),
              GetAdminDaclSecurityDescriptor(GENERIC_ALL)};
  }
}

absl::optional<CSecurityDesc> GetCurrentUserDefaultSecurityDescriptor() {
  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY))
    return absl::nullopt;

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner))
    return absl::nullopt;

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group))
    return absl::nullopt;

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl))
    return absl::nullopt;

  CSid sid_user;
  if (!token.GetUser(&sid_user))
    return absl::nullopt;
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL))
    return absl::nullopt;

  security_desc.SetDacl(dacl);

  return security_desc;
}

CSecurityDesc GetAdminDaclSecurityDescriptor(ACCESS_MASK accessmask) {
  CSecurityDesc sd;
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  sd.SetOwner(Sids::Admins());
  sd.SetGroup(Sids::Admins());
  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  return sd;
}

std::wstring GetAppClientsKey(const std::string& app_id) {
  return GetAppClientsKey(base::ASCIIToWide(app_id));
}

std::wstring GetAppClientsKey(const std::wstring& app_id) {
  return base::StrCat({CLIENTS_KEY, app_id});
}

std::wstring GetAppClientStateKey(const std::string& app_id) {
  return GetAppClientStateKey(base::ASCIIToWide(app_id));
}

std::wstring GetAppClientStateKey(const std::wstring& app_id) {
  return base::StrCat({CLIENT_STATE_KEY, app_id});
}

std::wstring GetAppCommandKey(const std::wstring& app_id,
                              const std::wstring& command_id) {
  return base::StrCat(
      {GetAppClientsKey(app_id), L"\\", kRegKeyCommands, L"\\", command_id});
}

std::wstring GetRegistryKeyClientsUpdater() {
  return GetAppClientsKey(kUpdaterAppId);
}

std::wstring GetRegistryKeyClientStateUpdater() {
  return GetAppClientStateKey(kUpdaterAppId);
}

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes == -1 || total_bytes == -1 || total_bytes == 0)
    return -1;
  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 * base::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
}

base::win::ScopedHandle GetUserTokenFromCurrentSessionId() {
  base::win::ScopedHandle token_handle;

  DWORD bytes_returned = 0;
  DWORD* session_id_ptr = nullptr;
  if (!::WTSQuerySessionInformation(
          WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId,
          reinterpret_cast<LPTSTR*>(&session_id_ptr), &bytes_returned)) {
    PLOG(ERROR) << "WTSQuerySessionInformation failed.";
    return token_handle;
  }

  DCHECK_EQ(bytes_returned, sizeof(*session_id_ptr));
  DWORD session_id = *session_id_ptr;
  ::WTSFreeMemory(session_id_ptr);
  VLOG(1) << "::WTSQuerySessionInformation session id: " << session_id;

  HANDLE token_handle_raw = nullptr;
  if (!::WTSQueryUserToken(session_id, &token_handle_raw)) {
    PLOG(ERROR) << "WTSQueryUserToken failed";
    return token_handle;
  }

  token_handle.Set(token_handle_raw);
  return token_handle;
}

bool PathOwnedByUser(const base::FilePath& path) {
  // TODO(crbug.com/1147094): Implement for Win.
  return true;
}

HResultOr<bool> IsTokenAdmin(HANDLE token) {
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID administrators_group = nullptr;
  if (!::AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &administrators_group)) {
    return base::unexpected(HRESULTFromLastError());
  }
  base::ScopedClosureRunner free_sid(
      base::BindOnce([](PSID sid) { ::FreeSid(sid); }, administrators_group));
  BOOL is_member = false;
  if (!::CheckTokenMembership(token, administrators_group, &is_member))
    return base::unexpected(HRESULTFromLastError());
  return base::ok(is_member);
}

// TODO(crbug.com/1212187): maybe handle filtered tokens.
HResultOr<bool> IsUserAdmin() {
  return IsTokenAdmin(NULL);
}

HResultOr<bool> IsUserNonElevatedAdmin() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &token))
    return base::unexpected(HRESULTFromLastError());
  bool is_user_non_elevated_admin = false;
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenElevationType,
                            &elevation_type, sizeof(elevation_type),
                            &size_returned)) {
    if (elevation_type == TokenElevationTypeLimited) {
      is_user_non_elevated_admin = true;
    }
  }
  return base::ok(is_user_non_elevated_admin);
}

HResultOr<bool> IsCOMCallerAdmin() {
  ScopedKernelHANDLE token;

  {
    HRESULT hr = ::CoImpersonateClient();
    if (hr == RPC_E_CALL_COMPLETE) {
      // RPC_E_CALL_COMPLETE indicates that the caller is in-proc.
      return base::ok(::IsUserAnAdmin());
    }

    if (FAILED(hr)) {
      return base::unexpected(hr);
    }

    base::ScopedClosureRunner co_revert_to_self(
        base::BindOnce([]() { ::CoRevertToSelf(); }));

    if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, TRUE,
                           ScopedKernelHANDLE::Receiver(token).get())) {
      hr = HRESULTFromLastError();
      LOG(ERROR) << __func__ << ": ::OpenThreadToken failed: " << std::hex
                 << hr;
      return base::unexpected(hr);
    }
  }

  HResultOr<bool> result = IsTokenAdmin(token.get());
  if (!result.has_value()) {
    HRESULT hr = result.error();
    DCHECK(FAILED(hr));
    LOG(ERROR) << __func__ << ": IsTokenAdmin failed: " << std::hex << hr;
  }
  return result;
}

bool IsUACOn() {
  // The presence of a split token definitively indicates that UAC is on. But
  // the absence of the token does not necessarily indicate that UAC is off.
  HResultOr<bool> is_split_token = IsUserRunningSplitToken();
  if (is_split_token.has_value() && is_split_token.value())
    return true;

  return IsExplorerRunningAtMediumOrLower();
}

bool IsElevatedWithUACOn() {
  HResultOr<bool> is_user_admin = IsUserAdmin();
  if (is_user_admin.has_value() && !is_user_admin.value())
    return false;

  return IsUACOn();
}

std::string GetUACState() {
  std::string s;

  HResultOr<bool> is_user_admin = IsUserAdmin();
  if (is_user_admin.has_value())
    base::StringAppendF(&s, "IsUserAdmin: %d, ", is_user_admin.value());

  HResultOr<bool> is_user_non_elevated_admin = IsUserNonElevatedAdmin();
  if (is_user_non_elevated_admin.has_value())
    base::StringAppendF(&s, "IsUserNonElevatedAdmin: %d, ",
                        is_user_non_elevated_admin.value());

  base::StringAppendF(&s, "IsUACOn: %d, ", IsUACOn());
  base::StringAppendF(&s, "IsElevatedWithUACOn: %d", IsElevatedWithUACOn());

  return s;
}

std::wstring GetServiceName(bool is_internal_service) {
  std::wstring service_name = GetServiceDisplayName(is_internal_service);
  service_name.erase(base::ranges::remove_if(service_name, isspace),
                     service_name.end());
  return service_name;
}

std::wstring GetServiceDisplayName(bool is_internal_service) {
  return base::StrCat(
      {base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
       is_internal_service ? kWindowsInternalServiceName : kWindowsServiceName,
       L" ", kUpdaterVersionUtf16});
}

REGSAM Wow6432(REGSAM access) {
  CHECK(access);

  return KEY_WOW64_32KEY | access;
}

HResultOr<DWORD> ShellExecuteAndWait(const base::FilePath& file_path,
                                     const std::wstring& parameters,
                                     const std::wstring& verb) {
  VLOG(1) << __func__ << ": path: " << file_path
          << ", parameters:" << parameters << ", verb:" << verb;
  DCHECK(!file_path.empty());

  const HWND hwnd = CreateForegroundParentWindowForUAC();
  const base::ScopedClosureRunner destroy_window(base::BindOnce(
      [](HWND hwnd) {
        if (hwnd)
          ::DestroyWindow(hwnd);
      },
      hwnd));

  SHELLEXECUTEINFO shell_execute_info = {};
  shell_execute_info.cbSize = sizeof(SHELLEXECUTEINFO);
  shell_execute_info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS |
                             SEE_MASK_NOZONECHECKS | SEE_MASK_NOASYNC;
  shell_execute_info.hProcess = NULL;
  shell_execute_info.hwnd = hwnd;
  shell_execute_info.lpVerb = verb.c_str();
  shell_execute_info.lpFile = file_path.value().c_str();
  shell_execute_info.lpParameters = parameters.c_str();
  shell_execute_info.lpDirectory = NULL;
  shell_execute_info.nShow = SW_SHOW;
  shell_execute_info.hInstApp = NULL;

  if (!::ShellExecuteEx(&shell_execute_info)) {
    const HRESULT hr = HRESULTFromLastError();
    VLOG(1) << __func__ << ": ::ShellExecuteEx failed: " << std::hex << hr;
    return base::unexpected(hr);
  }

  if (!shell_execute_info.hProcess) {
    VLOG(1) << __func__ << ": Started process, PID unknown";
    return base::ok(0);
  }

  const base::Process process(shell_execute_info.hProcess);
  const DWORD pid = process.Pid();
  VLOG(1) << __func__ << ": Started process, PID: " << pid;

  // Allow the spawned process to show windows in the foreground.
  if (!::AllowSetForegroundWindow(pid)) {
    LOG(WARNING) << __func__
                 << ": ::AllowSetForegroundWindow failed: " << ::GetLastError();
  }

  int ret_val = 0;
  if (!process.WaitForExit(&ret_val))
    return base::unexpected(HRESULTFromLastError());

  return base::ok(static_cast<DWORD>(ret_val));
}

HResultOr<DWORD> RunElevated(const base::FilePath& file_path,
                             const std::wstring& parameters) {
  return ShellExecuteAndWait(file_path, parameters, L"runas");
}

HRESULT RunDeElevated(const std::wstring& path,
                      const std::wstring& parameters) {
  Microsoft::WRL::ComPtr<IShellWindows> shell;
  HRESULT hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&shell));
  if (FAILED(hr))
    return hr;

  long hwnd = 0;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = shell->FindWindowSW(base::win::ScopedVariant(CSIDL_DESKTOP).AsInput(),
                           base::win::ScopedVariant().AsInput(), SWC_DESKTOP,
                           &hwnd, SWFO_NEEDDISPATCH, &dispatch);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IServiceProvider> service;
  hr = dispatch.As(&service);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellBrowser> browser;
  hr = service->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellView> view;
  hr = browser->QueryActiveShellView(&view);
  if (FAILED(hr))
    return hr;

  hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&dispatch));
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellFolderViewDual> folder;
  hr = dispatch.As(&folder);
  if (FAILED(hr))
    return hr;

  hr = folder->get_Application(&dispatch);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch;
  hr = dispatch.As(&shell_dispatch);
  if (FAILED(hr))
    return hr;

  return shell_dispatch->ShellExecute(
      base::win::ScopedBstr(path).Get(),
      base::win::ScopedVariant(parameters.c_str()),
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant);
}

absl::optional<base::FilePath> GetGoogleUpdateExePath(UpdaterScope scope) {
  base::FilePath goopdate_base_dir;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILESX86
                                  : base::DIR_LOCAL_APP_DATA,
                              &goopdate_base_dir)) {
    LOG(ERROR) << "Can't retrieve GoogleUpdate base directory.";
    return absl::nullopt;
  }

  base::FilePath goopdate_dir =
      goopdate_base_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII("Update");
  if (!base::CreateDirectory(goopdate_dir)) {
    LOG(ERROR) << "Can't create GoogleUpdate directory: " << goopdate_dir;
    return absl::nullopt;
  }

  return goopdate_dir.AppendASCII(base::WideToASCII(kLegacyExeName));
}

HRESULT DisableCOMExceptionHandling() {
  Microsoft::WRL::ComPtr<IGlobalOptions> options;
  HRESULT hr = ::CoCreateInstance(CLSID_GlobalOptions, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&options));
  if (FAILED(hr))
    return hr;
  return hr = options->Set(COMGLB_EXCEPTION_HANDLING,
                           COMGLB_EXCEPTION_DONOT_HANDLE);
}

std::wstring BuildMsiCommandLine(
    const std::wstring& arguments,
    const absl::optional<base::FilePath>& installer_data_file,
    const base::FilePath& msi_installer) {
  if (!msi_installer.MatchesExtension(L".msi")) {
    return std::wstring();
  }

  return base::StrCat(
      {L"msiexec ", arguments,
       installer_data_file
           ? base::StrCat(
                 {L" ",
                  base::UTF8ToWide(base::ToUpperASCII(kInstallerDataSwitch)),
                  L"=\"", installer_data_file->value(), L"\""})
           : L"",
       L" REBOOT=ReallySuppress /qn /i \"", msi_installer.value(),
       L"\" /log \"", msi_installer.value(), L".log\""});
}

std::wstring BuildExeCommandLine(
    const std::wstring& arguments,
    const absl::optional<base::FilePath>& installer_data_file,
    const base::FilePath& exe_installer) {
  if (!exe_installer.MatchesExtension(L".exe")) {
    return std::wstring();
  }

  return base::StrCat({base::CommandLine(exe_installer).GetCommandLineString(),
                       L" ", arguments, [&installer_data_file]() {
                         if (!installer_data_file)
                           return std::wstring();

                         base::CommandLine installer_data_args(
                             base::CommandLine::NO_PROGRAM);
                         installer_data_args.AppendSwitchPath(
                             kInstallerDataSwitch, *installer_data_file);
                         return installer_data_args.GetCommandLineString();
                       }()});
}

bool IsServiceRunning(const std::wstring& service_name) {
  ScopedScHandle scm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm.IsValid()) {
    LOG(ERROR) << "::OpenSCManager failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  ScopedScHandle service(
      ::OpenService(scm.Get(), service_name.c_str(), SERVICE_QUERY_STATUS));
  if (!service.IsValid()) {
    LOG(ERROR) << "::OpenService failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  SERVICE_STATUS status = {0};
  if (!::QueryServiceStatus(service.Get(), &status)) {
    LOG(ERROR) << "::QueryServiceStatus failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  VLOG(1) << "IsServiceRunning. service_name: " << service_name
          << ", status: " << std::hex << status.dwCurrentState;
  return status.dwCurrentState == SERVICE_RUNNING ||
         status.dwCurrentState == SERVICE_START_PENDING;
}

HKEY UpdaterScopeToHKeyRoot(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE
                                        : HKEY_CURRENT_USER;
}

absl::optional<OSVERSIONINFOEX> GetOSVersion() {
  // `::RtlGetVersion` is being used here instead of `::GetVersionEx`, because
  // the latter function can return the incorrect version if it is shimmed using
  // an app compat shim.
  using RtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOEX*);
  static const RtlGetVersion rtl_get_version = reinterpret_cast<RtlGetVersion>(
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "RtlGetVersion"));
  if (!rtl_get_version)
    return absl::nullopt;

  OSVERSIONINFOEX os_out = {};
  os_out.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  rtl_get_version(&os_out);
  if (!os_out.dwMajorVersion)
    return absl::nullopt;

  return os_out;
}

bool CompareOSVersions(const OSVERSIONINFOEX& os_version, BYTE oper) {
  DCHECK(oper);

  constexpr DWORD kOSTypeMask = VER_MAJORVERSION | VER_MINORVERSION |
                                VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;
  constexpr DWORD kBuildTypeMask = VER_BUILDNUMBER;

  // If the OS and the service pack match, return the build number comparison.
  return CompareOSVersionsInternal(os_version, kOSTypeMask, VER_EQUAL)
             ? CompareOSVersionsInternal(os_version, kBuildTypeMask, oper)
             : CompareOSVersionsInternal(os_version, kOSTypeMask, oper);
}

bool EnableSecureDllLoading() {
  static const auto set_default_dll_directories =
      reinterpret_cast<decltype(&::SetDefaultDllDirectories)>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "SetDefaultDllDirectories"));

  if (!set_default_dll_directories)
    return true;

#if defined(COMPONENT_BUILD)
  const DWORD directory_flags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
#else
  const DWORD directory_flags = LOAD_LIBRARY_SEARCH_SYSTEM32;
#endif

  return set_default_dll_directories(directory_flags);
}

bool EnableProcessHeapMetadataProtection() {
  if (!::HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr,
                            0)) {
    LOG(ERROR) << __func__
               << ": Failed to enable heap metadata protection: " << std::hex
               << HRESULTFromLastError();
    return false;
  }

  return true;
}

absl::optional<base::ScopedTempDir> CreateSecureTempDir() {
  base::FilePath temp_dir;
  if (!base::PathService::Get(::IsUserAnAdmin() ? int{base::DIR_PROGRAM_FILES}
                                                : int{base::DIR_TEMP},
                              &temp_dir)) {
    return absl::nullopt;
  }

  base::ScopedTempDir temp_path;
  if (!temp_path.CreateUniqueTempDirUnderPath(
          temp_dir.AppendASCII(COMPANY_SHORTNAME_STRING))) {
    return absl::nullopt;
  }

  return temp_path;
}

base::ScopedClosureRunner SignalShutdownEvent(UpdaterScope scope) {
  NamedObjectAttributes attr = GetNamedObjectAttributes(kShutdownEvent, scope);

  base::win::ScopedHandle shutdown_event_handle(
      ::CreateEvent(&attr.sa, true, false, attr.name.c_str()));
  if (!shutdown_event_handle.IsValid()) {
    VLOG(1) << __func__ << "Could not create the shutdown event: " << std::hex
            << HRESULTFromLastError();
    return {};
  }

  auto shutdown_event =
      std::make_unique<base::WaitableEvent>(std::move(shutdown_event_handle));
  shutdown_event->Signal();
  return base::ScopedClosureRunner(
      base::BindOnce(&base::WaitableEvent::Reset, std::move(shutdown_event)));
}

bool IsShutdownEventSignaled(UpdaterScope scope) {
  NamedObjectAttributes attr = GetNamedObjectAttributes(kShutdownEvent, scope);

  base::win::ScopedHandle event_handle(
      ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name.c_str()));
  if (!event_handle.IsValid())
    return false;

  base::WaitableEvent event(std::move(event_handle));
  return event.IsSignaled();
}

bool StopGoogleUpdateProcesses(UpdaterScope scope) {
  // Filters processes running under `path_prefix`.
  class PathPrefixProcessFilter : public base::ProcessFilter {
   public:
    explicit PathPrefixProcessFilter(const base::FilePath& path_prefix)
        : path_prefix_(path_prefix) {}

    bool Includes(const base::ProcessEntry& entry) const override {
      base::Process process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                          false, entry.th32ProcessID));
      if (!process.IsValid())
        return false;

      DWORD path_len = MAX_PATH;
      wchar_t path_string[MAX_PATH];
      if (!::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                       &path_len)) {
        return false;
      }

      return path_prefix_.IsParent(base::FilePath(path_string));
    }

   private:
    const base::FilePath path_prefix_;
  };

  constexpr base::TimeDelta kShutdownWaitSeconds = base::Seconds(45);

  absl::optional<base::FilePath> target = GetGoogleUpdateExePath(scope);
  if (!target)
    return false;

  PathPrefixProcessFilter path_prefix_filter(target->DirName());
  return base::CleanupProcesses(kLegacyExeName, kShutdownWaitSeconds, -1,
                                &path_prefix_filter);
}

}  // namespace updater
