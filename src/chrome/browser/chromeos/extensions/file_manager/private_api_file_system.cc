// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"

#include <sys/statvfs.h>
#include <sys/xattr.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/empty_trash_io_task.h"
#include "chrome/browser/ash/file_manager/extract_io_task.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/restore_io_task.h"
#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/zip_io_task.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_stream_md5_digester.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/constants/cryptohome.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_non_backed.h"

using ::ash::disks::DiskMountManager;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;
using storage::FileSystemURL;

namespace extensions {
namespace {

using file_manager::Volume;
using file_manager::VolumeManager;

std::string Redact(const base::StringPiece s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

const char kRootPath[] = "/";

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsAsync(const base::FilePath& mount_path,
                       uint64_t* total_size,
                       uint64_t* remaining_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0)
    *total_size = size;
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0)
    *remaining_size = size;
}

// Retrieves the maximum file name length of the file system of |path|.
// Returns a default of 255 if it could not be queried.
size_t GetFileNameMaxLengthAsync(const std::string& path) {
  struct statvfs stat = {};
  if (HANDLE_EINTR(statvfs(path.c_str(), &stat)) != 0) {
    // The filesystem seems not supporting statvfs(). Assume it to be a commonly
    // used bound 255, and log the failure.
    LOG(ERROR) << "Cannot statvfs() the name length limit for: " << path;
    return 255;
  }
  return stat.f_namemax;
}

bool GetFileExtendedAttribute(const base::FilePath& path,
                              const char* name,
                              std::vector<char>* value) {
  ssize_t len = getxattr(path.value().c_str(), name, nullptr, 0);
  if (len < 0) {
    PLOG_IF(ERROR, errno != ENODATA) << "getxattr: " << path;
    return false;
  }
  value->resize(len);
  if (getxattr(path.value().c_str(), name, value->data(), len) != len) {
    PLOG(ERROR) << "getxattr: " << path;
    return false;
  }
  return true;
}

// Returns EventRouter for the |profile_id| if available.
file_manager::EventRouter* GetEventRouterByProfileId(void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return nullptr;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  return file_manager::EventRouterFactory::GetForProfile(profile);
}

// Notifies the copy progress to extensions via event router.
void NotifyCopyProgress(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID operation_id,
    file_manager::FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router) {
    event_router->OnCopyProgress(operation_id, type, source_url.ToGURL(),
                                 destination_url.ToGURL(), size);
  }
}

// Callback invoked periodically on progress update of Copy().
void OnCopyProgress(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID* operation_id,
    file_manager::FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NotifyCopyProgress, profile_id, *operation_id,
                                type, source_url, destination_url, size));
}

// Notifies the copy completion to extensions via event router.
void NotifyCopyCompletion(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router)
    event_router->OnCopyCompleted(operation_id, source_url.ToGURL(),
                                  destination_url.ToGURL(), error);
}

// Callback invoked upon completion of Copy() (regardless of succeeded or
// failed).
void OnCopyCompleted(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID* operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NotifyCopyCompletion, profile_id, *operation_id,
                     source_url, destination_url, error));
}

// Notifies the start of a copy to extensions via event router.
void NotifyCopyStart(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t space_needed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router)
    event_router->OnCopyStarted(operation_id, source_url.ToGURL(),
                                destination_url.ToGURL(), space_needed);
}

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    void* profile_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t space_needed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note: |operation_id| is owned by the callback for
  // FileSystemOperationRunner::Copy(). It is always called in the next message
  // loop or later, so at least during this invocation it should alive.
  //
  // TODO(yawano): change ERROR_BEHAVIOR_ABORT to ERROR_BEHAVIOR_SKIP after
  //     error messages of individual operations become visible in the Files app
  //     UI.
  storage::FileSystemOperationRunner::OperationID* operation_id =
      new storage::FileSystemOperationRunner::OperationID;
  *operation_id = file_system_context->operation_runner()->Copy(
      source_url, destination_url,
      storage::FileSystemOperation::CopyOrMoveOptionSet(
          storage::FileSystemOperation::CopyOrMoveOption::
              kPreserveLastModified),
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<file_manager::FileManagerCopyOrMoveHookDelegate>(
          base::BindRepeating(&OnCopyProgress, profile_id,
                              base::Unretained(operation_id))),
      base::BindOnce(&OnCopyCompleted, profile_id, base::Owned(operation_id),
                     source_url, destination_url));
  // Notify the start of copy to send total size.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NotifyCopyStart, profile_id, *operation_id,
                                source_url, destination_url, space_needed));
  return *operation_id;
}

void OnCopyCancelled(base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // We just ignore the status if the copy is actually cancelled or not,
  // because failing cancellation means the operation is not running now.
  DLOG_IF(WARNING, error != base::File::FILE_OK)
      << "Failed to cancel copy: " << error;
}

// Cancels the running copy operation identified by |operation_id|.
void CancelCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemOperationRunner::OperationID operation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->Cancel(
      operation_id, base::BindOnce(&OnCopyCancelled));
}

// Converts a status code to a bool value and calls the |callback| with it.
void StatusCallbackToResponseCallback(base::OnceCallback<void(bool)> callback,
                                      base::File::Error result) {
  std::move(callback).Run(result == base::File::FILE_OK);
}

// Calls a response callback (on the UI thread) with a file content hash
// computed on the IO thread.
void ComputeChecksumRespondOnUIThread(
    base::OnceCallback<void(std::string)> callback,
    std::string hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(hash)));
}

// Calls a response callback on the UI thread.
void GetFileMetadataRespondOnUIThread(
    storage::FileSystemOperation::GetMetadataCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, file_info));
}

// Construct a case-insensitive fnmatch query from |query|. E.g.  for abc123,
// the result would be *[aA][bB][cC]123*.
std::string CreateFnmatchQuery(const std::string& query) {
  std::vector<std::string> query_pieces = {"*"};
  size_t sequence_start = 0;
  for (size_t i = 0; i < query.size(); ++i) {
    if (isalpha(query[i])) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }
      std::string piece("[");
      piece.resize(4);
      piece[1] = tolower(query[i]);
      piece[2] = toupper(query[i]);
      piece[3] = ']';
      query_pieces.push_back(std::move(piece));
      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  query_pieces.push_back("*");

  return base::StrCat(query_pieces);
}

std::vector<std::pair<base::FilePath, bool>> SearchByPattern(
    const base::FilePath& root,
    const std::string& query,
    size_t max_results) {
  std::vector<std::pair<base::FilePath, bool>> prefix_matches;
  std::vector<std::pair<base::FilePath, bool>> other_matches;

  base::FileEnumerator enumerator(
      root, true,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES,
      CreateFnmatchQuery(query), base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (base::StartsWith(path.BaseName().value(), query,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      prefix_matches.emplace_back(path, enumerator.GetInfo().IsDirectory());
      if (max_results && prefix_matches.size() == max_results) {
        return prefix_matches;
      }
      continue;
    }
    if (!max_results ||
        prefix_matches.size() + other_matches.size() < max_results) {
      other_matches.emplace_back(path, enumerator.GetInfo().IsDirectory());
    }
  }
  prefix_matches.insert(
      prefix_matches.end(), other_matches.begin(),
      other_matches.begin() +
          std::min(max_results - prefix_matches.size(), other_matches.size()));

  return prefix_matches;
}

ash::disks::FormatFileSystemType ApiFormatFileSystemToChromeEnum(
    api::file_manager_private::FormatFileSystemType filesystem) {
  switch (filesystem) {
    case api::file_manager_private::FORMAT_FILE_SYSTEM_TYPE_NONE:
      return ash::disks::FormatFileSystemType::kUnknown;
    case api::file_manager_private::FORMAT_FILE_SYSTEM_TYPE_VFAT:
      return ash::disks::FormatFileSystemType::kVfat;
    case api::file_manager_private::FORMAT_FILE_SYSTEM_TYPE_EXFAT:
      return ash::disks::FormatFileSystemType::kExfat;
    case api::file_manager_private::FORMAT_FILE_SYSTEM_TYPE_NTFS:
      return ash::disks::FormatFileSystemType::kNtfs;
  }
  NOTREACHED() << "Unknown format filesystem " << filesystem;
  return ash::disks::FormatFileSystemType::kUnknown;
}

absl::optional<file_manager::io_task::OperationType> IOTaskTypeToChromeEnum(
    api::file_manager_private::IOTaskType type) {
  switch (type) {
    case api::file_manager_private::IO_TASK_TYPE_COPY:
      return file_manager::io_task::OperationType::kCopy;
    case api::file_manager_private::IO_TASK_TYPE_DELETE:
      return file_manager::io_task::OperationType::kDelete;
    case api::file_manager_private::IO_TASK_TYPE_EMPTY_TRASH:
      return file_manager::io_task::OperationType::kEmptyTrash;
    case api::file_manager_private::IO_TASK_TYPE_EXTRACT:
      return file_manager::io_task::OperationType::kExtract;
    case api::file_manager_private::IO_TASK_TYPE_MOVE:
      return file_manager::io_task::OperationType::kMove;
    case api::file_manager_private::IO_TASK_TYPE_RESTORE:
      return file_manager::io_task::OperationType::kRestore;
    case api::file_manager_private::IO_TASK_TYPE_RESTORE_TO_DESTINATION:
      return file_manager::io_task::OperationType::kRestoreToDestination;
    case api::file_manager_private::IO_TASK_TYPE_TRASH:
      return file_manager::io_task::OperationType::kTrash;
    case api::file_manager_private::IO_TASK_TYPE_ZIP:
      return file_manager::io_task::OperationType::kZip;
    case api::file_manager_private::IO_TASK_TYPE_NONE:
      return {};
  }
  NOTREACHED() << "Unknown I/O task type " << type;
  return {};
}

extensions::api::file_manager_private::DlpLevel DlpRulesManagerLevelToApiEnum(
    policy::DlpRulesManager::Level level) {
  using extensions::api::file_manager_private::DlpLevel;
  switch (level) {
    case policy::DlpRulesManager::Level::kAllow:
      return DlpLevel::DLP_LEVEL_ALLOW;
    case policy::DlpRulesManager::Level::kBlock:
      return DlpLevel::DLP_LEVEL_BLOCK;
    case policy::DlpRulesManager::Level::kWarn:
      return DlpLevel::DLP_LEVEL_WARN;
    case policy::DlpRulesManager::Level::kReport:
      return DlpLevel::DLP_LEVEL_REPORT;
    case policy::DlpRulesManager::Level::kNotSet:
      NOTREACHED() << "DLP level not set.";
      return DlpLevel::DLP_LEVEL_NONE;
  }
  NOTREACHED() << "Unknown DLP level.";
  return {};
}

extensions::api::file_manager_private::VolumeType
DlpRulesManagerComponentToApiEnum(
    policy::DlpRulesManager::Component component) {
  using ::extensions::api::file_manager_private::VolumeType;
  using Component = ::policy::DlpRulesManager::Component;
  switch (component) {
    case Component::kArc:
      return VolumeType::VOLUME_TYPE_ANDROID_FILES;
    case Component::kCrostini:
      return VolumeType::VOLUME_TYPE_CROSTINI;
    case Component::kPluginVm:
      return VolumeType::VOLUME_TYPE_GUEST_OS;
    case Component::kUsb:
      return VolumeType::VOLUME_TYPE_REMOVABLE;
    case Component::kDrive:
      return VolumeType::VOLUME_TYPE_DRIVE;
    case Component::kUnknownComponent:
      NOTREACHED() << "DLP component not set.";
      return {};
  }
  NOTREACHED() << "Unknown component type.";
  return {};
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateEnableExternalFileSchemeFunction::Run() {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      render_frame_host()->GetProcess()->GetID(), content::kExternalFileScheme);
  return RespondNow(WithArguments());
}

FileManagerPrivateGrantAccessFunction::FileManagerPrivateGrantAccessFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGrantAccessFunction::Run() {
  using extensions::api::file_manager_private::GrantAccess::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    if (profile->IsOffTheRecord())
      continue;
    storage::FileSystemContext* const context =
        file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                             source_url());
    for (const auto& url : params->entry_urls) {
      const storage::FileSystemURL file_system_url =
          context->CrackURLInFirstPartyContext(GURL(url));
      // Grant permissions only to valid urls backed by the external file system
      // backend.
      if (!file_system_url.is_valid() ||
          file_system_url.mount_type() != storage::kFileSystemTypeExternal) {
        continue;
      }
      backend->GrantFileAccessToOrigin(url::Origin::Create(source_url()),
                                       file_system_url.virtual_path());
      content::ChildProcessSecurityPolicy::GetInstance()
          ->GrantCreateReadWriteFile(render_frame_host()->GetProcess()->GetID(),
                                     file_system_url.path());
    }
  }
  return RespondNow(WithArguments());
}

namespace {

void PostResponseCallbackTaskToUIThread(
    FileWatchFunctionBase::ResponseCallback callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PostNotificationCallbackTaskToUIThread(
    storage::WatcherManager::NotificationCallback callback,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), type));
}

}  // namespace

void FileWatchFunctionBase::RespondWith(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success) {
    Respond(WithArguments(success));
  } else {
    Respond(Error(""));
  }
}

ExtensionFunction::ResponseAction FileWatchFunctionBase::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return RespondNow(Error("Invalid state"));

  // First param is url of a file to watch.
  if (args().empty() || !args()[0].is_string() || args()[0].GetString().empty())
    return RespondNow(Error("Empty watch URL"));
  const std::string& url = args()[0].GetString();

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  const FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(GURL(url));
  if (file_system_url.path().empty()) {
    auto result_list = std::make_unique<base::ListValue>();
    result_list->Append(false);
    return RespondNow(Error("Invalid URL"));
  }

  // For removeFileWatch() we can't validate the volume because it might have
  // been unmounted.
  if (IsAddWatch()) {
    VolumeManager* const volume_manager = VolumeManager::Get(profile);
    if (!volume_manager)
      return RespondNow(Error("Cannot find VolumeManager"));

    const base::WeakPtr<Volume> volume =
        volume_manager->FindVolumeFromPath(file_system_url.path());
    if (!volume)
      return RespondNow(
          Error("Cannot find volume *", Redact(file_system_url.path())));

    if (!volume->watchable())
      return RespondNow(Error("Volume is not watchable"));
  }

  file_manager::EventRouter* const event_router =
      file_manager::EventRouterFactory::GetForProfile(profile);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileWatchFunctionBase::RunAsyncOnIOThread,
                                this, file_system_context, file_system_url,
                                event_router->GetWeakPtr()));
  return RespondLater();
}

void FileWatchFunctionBase::RunAsyncOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_system_url,
    base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::WatcherManager* const watcher_manager =
      file_system_context->GetWatcherManager(file_system_url.type());

  if (!watcher_manager) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileWatchFunctionBase::PerformFallbackFileWatchOperationOnUIThread,
            this, file_system_url, event_router));
    return;
  }

  PerformFileWatchOperationOnIOThread(file_system_context, watcher_manager,
                                      file_system_url, event_router);
}

void FileManagerPrivateInternalAddFileWatchFunction::
    PerformFileWatchOperationOnIOThread(
        scoped_refptr<storage::FileSystemContext> file_system_context,
        storage::WatcherManager* watcher_manager,
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  watcher_manager->AddWatcher(
      file_system_url, false /* recursive */,
      base::BindOnce(
          &StatusCallbackToResponseCallback,
          base::BindOnce(
              &PostResponseCallbackTaskToUIThread,
              base::BindOnce(&FileWatchFunctionBase::RespondWith, this))),
      base::BindRepeating(
          &PostNotificationCallbackTaskToUIThread,
          base::BindRepeating(
              &file_manager::EventRouter::OnWatcherManagerNotification,
              event_router, file_system_url,
              url::Origin::Create(source_url()))));
}

void FileManagerPrivateInternalAddFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->AddFileWatch(
      file_system_url.path(), file_system_url.virtual_path(),
      url::Origin::Create(source_url()),
      base::BindOnce(&FileWatchFunctionBase::RespondWith, this));
}

void FileManagerPrivateInternalRemoveFileWatchFunction::
    PerformFileWatchOperationOnIOThread(
        scoped_refptr<storage::FileSystemContext> file_system_context,
        storage::WatcherManager* watcher_manager,
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  watcher_manager->RemoveWatcher(
      file_system_url, false /* recursive */,
      base::BindOnce(
          &StatusCallbackToResponseCallback,
          base::BindOnce(
              &PostResponseCallbackTaskToUIThread,
              base::BindOnce(&FileWatchFunctionBase::RespondWith, this))));
}

void FileManagerPrivateInternalRemoveFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->RemoveFileWatch(file_system_url.path(),
                                url::Origin::Create(source_url()));
  RespondWith(true);
}

bool FileManagerPrivateInternalAddFileWatchFunction::IsAddWatch() {
  return true;
}

bool FileManagerPrivateInternalRemoveFileWatchFunction::IsAddWatch() {
  return false;
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetSizeStatsFunction::Run() {
  using extensions::api::file_manager_private::GetSizeStats::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager)
    return RespondNow(Error("Cannot find VolumeManager"));

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));

  // For fusebox volumes, get the underlying (aka regular) volume.
  const auto fusebox = base::StringPiece(file_manager::util::kFuseBox);
  if (base::StartsWith(volume->file_system_type(), fusebox)) {
    std::string volume_id = params->volume_id;

    if ((volume->type() == file_manager::VOLUME_TYPE_MTP) ||
        (volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER)) {
      volume_id = volume_id.substr(fusebox.length());
    } else if (volume->type() == file_manager::VOLUME_TYPE_PROVIDED) {
      // NB: FileManagerPrivate.GetSizeStats is not called by files app JS
      // because regular PROVIDED volumes do not support size stats.
      volume_manager->ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(&volume_id);
    }

    volume = volume_manager->FindVolumeById(volume_id);
    if (!volume.get()) {
      return RespondNow(Error("Cannot find volume with ID *", volume_id));
    }
  }

  if (volume->type() == file_manager::VOLUME_TYPE_MTP) {
    // Resolve storage_name.
    storage_monitor::StorageMonitor* storage_monitor =
        storage_monitor::StorageMonitor::GetInstance();
    storage_monitor::StorageInfo info;
    storage_monitor->GetStorageInfoForPath(volume->mount_path(), &info);
    std::string storage_name;
    base::RemoveChars(info.location(), kRootPath, &storage_name);
    DCHECK(!storage_name.empty());

    // Get MTP StorageInfo.
    auto* manager = storage_monitor->media_transfer_protocol_manager();
    manager->GetStorageInfoFromDevice(
        storage_name,
        base::BindOnce(
            &FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace,
            this));
  } else if (volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER) {
    std::string authority;
    std::string root_document_id;
    if (!arc::ParseDocumentsProviderPath(volume->mount_path(), &authority,
                                         &root_document_id)) {
      return RespondNow(Error("File path was invalid"));
    }

    // Get DocumentsProvider's root available and capacity sizes in bytes.
    auto* root_map = arc::ArcDocumentsProviderRootMap::GetForBrowserContext(
        browser_context());
    if (!root_map) {
      return RespondNow(Error("File not found"));
    }
    auto* root = root_map->Lookup(authority, root_document_id);
    if (!root) {
      return RespondNow(Error("File not found"));
    }
    root->GetRootSize(base::BindOnce(&FileManagerPrivateGetSizeStatsFunction::
                                         OnGetDocumentsProviderAvailableSpace,
                                     this));
  } else if (volume->type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE) {
    Profile* const profile = Profile::FromBrowserContext(browser_context());
    drive::DriveIntegrationService* integration_service =
        drive::util::GetIntegrationServiceByProfile(profile);
    if (!integration_service) {
      return RespondNow(Error("Drive not available"));
    }
    integration_service->GetQuotaUsage(base::BindOnce(
        &FileManagerPrivateGetSizeStatsFunction::OnGetDriveQuotaUsage, this));
  } else {
    uint64_t* total_size = new uint64_t(0);
    uint64_t* remaining_size = new uint64_t(0);
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetSizeStatsAsync, volume->mount_path(), total_size,
                       remaining_size),
        base::BindOnce(&FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats,
                       this, base::Owned(total_size),
                       base::Owned(remaining_size)));
  }
  return RespondLater();
}

void FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace(
    device::mojom::MtpStorageInfoPtr mtp_storage_info,
    const bool error) {
  if (error) {
    // If stats couldn't be gotten from MTP volume, result should be left
    // undefined same as we do for Drive.
    Respond(WithArguments());
    return;
  }

  const uint64_t max_capacity = mtp_storage_info->max_capacity;
  const uint64_t free_space_in_bytes = mtp_storage_info->free_space_in_bytes;
  OnGetSizeStats(&max_capacity, &free_space_in_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::
    OnGetDocumentsProviderAvailableSpace(const bool error,
                                         const uint64_t available_bytes,
                                         const uint64_t capacity_bytes) {
  if (error) {
    // If stats was not successfully retrieved from DocumentsProvider volume,
    // result should be left undefined same as we do for Drive.
    Respond(WithArguments());
    return;
  }
  OnGetSizeStats(&capacity_bytes, &available_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::OnGetDriveQuotaUsage(
    drive::FileError error,
    drivefs::mojom::QuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    Respond(WithArguments());
    return;
  }
  OnGetSizeStats(&usage->total_cloud_bytes, &usage->free_cloud_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats(
    const uint64_t* total_size,
    const uint64_t* remaining_size) {
  base::Value::Dict sizes;
  sizes.Set("totalSize", static_cast<double>(*total_size));
  sizes.Set("remainingSize", static_cast<double>(*remaining_size));
  Respond(WithArguments(std::move(sizes)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDriveQuotaMetadataFunction::Run() {
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    return RespondNow(Error("Drive not available"));
  }
  integration_service->GetPooledQuotaUsage(base::BindOnce(
      &FileManagerPrivateGetDriveQuotaMetadataFunction::OnGetDriveQuotaMetadata,
      this));

  return RespondLater();
}

void FileManagerPrivateGetDriveQuotaMetadataFunction::OnGetDriveQuotaMetadata(
    drive::FileError error,
    drivefs::mojom::PooledQuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    Respond(WithArguments());
    return;
  }

  api::file_manager_private::DriveQuotaMetadata quotaMetadata;

  quotaMetadata.user_type =
      usage->user_type == drivefs::mojom::UserType::kUnmanaged
          ? api::file_manager_private::UserType::USER_TYPE_KUNMANAGED
          : api::file_manager_private::UserType::USER_TYPE_KORGANIZATION;
  quotaMetadata.used_user_bytes = static_cast<double>(usage->used_user_bytes);
  quotaMetadata.total_user_bytes = static_cast<double>(usage->total_user_bytes);
  quotaMetadata.organization_limit_exceeded =
      usage->organization_limit_exceeded;
  quotaMetadata.organization_name = usage->organization_name;

  Respond(ArgumentList(
      api::file_manager_private::GetDriveQuotaMetadata::Results::Create(
          quotaMetadata)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalValidatePathNameLengthFunction::Run() {
  using extensions::api::file_manager_private_internal::ValidatePathNameLength::
      Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(
          GURL(params->parent_url)));
  if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
    return RespondNow(Error("Invalid URL"));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFileNameMaxLengthAsync,
                     file_system_url.path().AsUTF8Unsafe()),
      base::BindOnce(&FileManagerPrivateInternalValidatePathNameLengthFunction::
                         OnFilePathLimitRetrieved,
                     this, params->name.size()));
  return RespondLater();
}

void FileManagerPrivateInternalValidatePathNameLengthFunction::
    OnFilePathLimitRetrieved(size_t current_length, size_t max_length) {
  Respond(WithArguments(current_length <= max_length));
}

ExtensionFunction::ResponseAction
FileManagerPrivateFormatVolumeFunction::Run() {
  using extensions::api::file_manager_private::FormatVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager)
    return RespondNow(Error("Cannot find VolumeManager"));

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume)
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));

  DiskMountManager::GetInstance()->FormatMountedDevice(
      volume->mount_path().AsUTF8Unsafe(),
      ApiFormatFileSystemToChromeEnum(params->filesystem),
      params->volume_label);
  return RespondNow(WithArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateSinglePartitionFormatFunction::Run() {
  using extensions::api::file_manager_private::SinglePartitionFormat::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const DiskMountManager::Disks& disks =
      DiskMountManager::GetInstance()->disks();

  DiskMountManager::Disks::const_iterator it = disks.begin();
  for (; it != disks.end(); ++it) {
    if (it->get()->storage_device_path() == params->device_storage_path &&
        it->get()->is_parent()) {
      break;
    }
  }

  if (it == disks.end()) {
    return RespondNow(Error("Device not found"));
  }

  const ash::disks::Disk* const device_disk = it->get();
  DCHECK(device_disk);

  if (device_disk->is_read_only()) {
    return RespondNow(Error("Invalid device"));
  }

  DiskMountManager::GetInstance()->SinglePartitionFormatDevice(
      device_disk->device_path(),
      ApiFormatFileSystemToChromeEnum(params->filesystem),
      params->volume_label);
  return RespondNow(WithArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateRenameVolumeFunction::Run() {
  using extensions::api::file_manager_private::RenameVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager)
    return RespondNow(Error("Cannot find VolumeManager"));

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume)
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));

  DiskMountManager::GetInstance()->RenameMountedDevice(
      volume->mount_path().AsUTF8Unsafe(), params->new_name);
  return RespondNow(WithArguments());
}

namespace {

// Obtains file size of URL.
void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(
      url, fields,
      base::BindOnce(&GetFileMetadataRespondOnUIThread, std::move(callback)));
}

// Gets the available space of the |paths|, stopping on the first path that
// doesn't exist.
std::vector<int64_t> GetLocalDiskSpaces(
    const std::vector<base::FilePath>& paths) {
  std::vector<int64_t> result;
  for (const auto& path : paths) {
    if (!base::PathExists(path)) {
      break;
    }
    result.push_back(base::SysInfo::AmountOfFreeDiskSpace(path));
  }
  return result;
}

}  // namespace

FileManagerPrivateInternalGetDisallowedTransfersFunction::
    FileManagerPrivateInternalGetDisallowedTransfersFunction() = default;

FileManagerPrivateInternalGetDisallowedTransfersFunction::
    ~FileManagerPrivateInternalGetDisallowedTransfersFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDisallowedTransfersFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private_internal::GetDisallowedTransfers::
      Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  profile_ = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());

  for (const std::string& url : params->entries) {
    FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));
    if (!file_system_url.is_valid()) {
      return RespondNow(Error("File URL was invalid"));
    }
    source_urls_.push_back(file_system_url);
  }

  destination_url_ = file_system_context->CrackURLInFirstPartyContext(
      GURL(params->destination_entry));
  if (!destination_url_.is_valid()) {
    return RespondNow(Error("File URL was invalid"));
  }

  policy::DlpFilesController* files_controller =
      rules_manager->GetDlpFilesController();
  files_controller->GetDisallowedTransfers(
      source_urls_, destination_url_,
      base::BindOnce(&FileManagerPrivateInternalGetDisallowedTransfersFunction::
                         OnGetDisallowedFiles,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalGetDisallowedTransfersFunction::
    OnGetDisallowedFiles(std::vector<storage::FileSystemURL> disallowed_files) {
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : disallowed_files) {
    file_manager::util::FileDefinition file_definition;
    // Disallowed transfers lists regular files not directories.
    file_definition.is_directory = false;
    file_definition.virtual_path = file.virtual_path();
    file_definition.absolute_path = file.path();
    file_definition_list.emplace_back(std::move(file_definition));
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile_,
                                                           source_url()),
      url::Origin::Create(source_url().DeprecatedGetOriginAsURL()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(&FileManagerPrivateInternalGetDisallowedTransfersFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this));
}

void FileManagerPrivateInternalGetDisallowedTransfersFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK(entry_definition_list);

  Respond(
      WithArguments(file_manager::util::ConvertEntryDefinitionListToListValue(
          *entry_definition_list)));
}

FileManagerPrivateInternalGetDlpMetadataFunction::
    FileManagerPrivateInternalGetDlpMetadataFunction() = default;

FileManagerPrivateInternalGetDlpMetadataFunction::
    ~FileManagerPrivateInternalGetDlpMetadataFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDlpMetadataFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private_internal::GetDlpMetadata::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  for (const std::string& url : params->entries) {
    FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));
    if (!file_system_url.is_valid()) {
      return RespondNow(Error("File URL was invalid"));
    }
    source_urls_.push_back(file_system_url);
  }

  policy::DlpFilesController* files_controller =
      rules_manager->GetDlpFilesController();
  files_controller->GetDlpMetadata(
      source_urls_,
      base::BindOnce(
          &FileManagerPrivateInternalGetDlpMetadataFunction::OnGetDlpMetadata,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalGetDlpMetadataFunction::OnGetDlpMetadata(
    std::vector<policy::DlpFilesController::DlpFileMetadata>
        dlp_metadata_list) {
  using extensions::api::file_manager_private::DlpMetadata;
  std::vector<DlpMetadata> converted_list;
  for (const auto& md : dlp_metadata_list) {
    DlpMetadata metadata;
    metadata.is_dlp_restricted = md.is_dlp_restricted;
    metadata.source_url = md.source_url;
    converted_list.emplace_back(std::move(metadata));
  }
  Respond(ArgumentList(
      api::file_manager_private_internal::GetDlpMetadata::Results::Create(
          converted_list)));
}

FileManagerPrivateGetDlpRestrictionDetailsFunction::
    FileManagerPrivateGetDlpRestrictionDetailsFunction() = default;

FileManagerPrivateGetDlpRestrictionDetailsFunction::
    ~FileManagerPrivateGetDlpRestrictionDetailsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetDlpRestrictionDetailsFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private::GetDlpRestrictionDetails::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  policy::DlpFilesController* files_controller =
      rules_manager->GetDlpFilesController();
  const std::vector<policy::DlpFilesController::DlpFileRestrictionDetails>
      dlp_restriction_details =
          files_controller->GetDlpRestrictionDetails(params->source_url);

  using extensions::api::file_manager_private::DlpRestrictionDetails;
  std::vector<DlpRestrictionDetails> converted_list;

  for (const auto& [level, urls, components] : dlp_restriction_details) {
    DlpRestrictionDetails details;
    details.level = DlpRulesManagerLevelToApiEnum(level);
    base::ranges::move(urls.begin(), urls.end(),
                       std::back_inserter(details.urls));
    for (const auto& component : components) {
      details.components.push_back(
          DlpRulesManagerComponentToApiEnum(component));
    }
    converted_list.emplace_back(std::move(details));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDlpRestrictionDetails::Results::Create(
          converted_list)));
}

FileManagerPrivateGetDlpBlockedComponentsFunction::
    FileManagerPrivateGetDlpBlockedComponentsFunction() = default;

FileManagerPrivateGetDlpBlockedComponentsFunction::
    ~FileManagerPrivateGetDlpBlockedComponentsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetDlpBlockedComponentsFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  policy::DlpFilesController* files_controller;
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !(files_controller = rules_manager->GetDlpFilesController())) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private::GetDlpBlockedComponents::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::vector<policy::DlpRulesManager::Component> components =
      files_controller->GetBlockedComponents(params->source_url);

  using extensions::api::file_manager_private::VolumeType;
  std::vector<VolumeType> converted_list;

  for (const auto& component : components) {
    converted_list.emplace_back(DlpRulesManagerComponentToApiEnum(component));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDlpBlockedComponents::Results::Create(
          converted_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDialogCallerFunction::Run() {
  absl::optional<policy::DlpFilesController::DlpFileDestination> caller =
      SelectFileDialogExtensionUserData::GetDialogCallerForWebContents(
          GetSenderWebContents());
  base::Value::Dict info;
  if (caller.has_value()) {
    if (caller->url_or_path.has_value()) {
      info.Set("url", caller->url_or_path.value());
    }
    if (caller->component.has_value()) {
      info.Set("component",
               DlpRulesManagerComponentToApiEnum(caller->component.value()));
    }
  }

  return RespondNow(WithArguments(std::move(info)));
}

FileManagerPrivateInternalStartCopyFunction::
    FileManagerPrivateInternalStartCopyFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalStartCopyFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::StartCopy::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  profile_ = Profile::FromBrowserContext(browser_context());

  if (params->url.empty() || params->parent_url.empty() ||
      params->new_name.empty()) {
    // Error code in format of DOMError.name.
    return RespondNow(Error("EncodingError"));
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());

  // |parent| may have a trailing slash if it is a root directory.
  std::string destination_url_string = params->parent_url;
  if (destination_url_string.back() != '/')
    destination_url_string += '/';
  destination_url_string += base::EscapePath(params->new_name);

  source_url_ =
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url));
  destination_url_ = file_system_context->CrackURLInFirstPartyContext(
      GURL(destination_url_string));

  if (!source_url_.is_valid() || !destination_url_.is_valid()) {
    // Error code in format of DOMError.name.
    return RespondNow(Error("EncodingError"));
  }

  // Check how much space we need for the copy operation.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetFileMetadataOnIOThread, file_system_context, source_url_,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE,
          base::BindOnce(&FileManagerPrivateInternalStartCopyFunction::
                             RunAfterGetFileMetadata,
                         this)));
  return RespondLater();
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterGetFileMetadata(
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result != base::File::FILE_OK) {
    Respond(Error("NotFoundError"));
    return;
  }

  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  std::vector<base::FilePath> destination_dirs;
  if (drive_integration_service &&
      drive_integration_service->GetMountPointPath().IsParent(
          destination_url_.path())) {
    // Google Drive's cache is limited by the available space on the local disk
    // as well as the cloud storage.
    destination_dirs.push_back(
        file_manager::util::GetMyFilesFolderForProfile(profile_));
  }
  destination_dirs.push_back(destination_url_.path().DirName());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetLocalDiskSpaces, std::move(destination_dirs)),
      base::BindOnce(
          &FileManagerPrivateInternalStartCopyFunction::RunAfterCheckDiskSpace,
          this, file_info.size));
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterCheckDiskSpace(
    int64_t space_needed,
    const std::vector<int64_t>& spaces_available) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (spaces_available.empty()) {
    // It might be a virtual path. In this case we just assume that it has
    // enough space.
    RunAfterFreeDiskSpace(true, space_needed);
    return;
  }
  // If the target is not internal storage or Drive, succeed if sufficient space
  // is available.
  if (destination_url_.filesystem_id() !=
          file_manager::util::GetDownloadsMountPointName(profile_) &&
      !(drive_integration_service &&
        drive_integration_service->GetMountPointPath().IsParent(
            destination_url_.path()))) {
    RunAfterFreeDiskSpace(spaces_available[0] > space_needed, space_needed);
    return;
  }

  // If there isn't enough cloud space, fail.
  if (spaces_available.size() > 1 && spaces_available[1] < space_needed) {
    RunAfterFreeDiskSpace(false, space_needed);
    return;
  }

  // If the destination directory is local hard drive or Google Drive we
  // must leave some additional space to make sure we don't break the system.
  if (spaces_available[0] - cryptohome::kMinFreeSpaceInBytes > space_needed) {
    RunAfterFreeDiskSpace(true, space_needed);
    return;
  }
  RunAfterFreeDiskSpace(false, space_needed);
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterFreeDiskSpace(
    bool available,
    int64_t space_needed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!render_frame_host()) {  // crbug.com/1261282
    Respond(WithArguments());
    return;
  }

  if (!available) {
    Respond(Error("QuotaExceededError"));
    return;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartCopyOnIOThread, profile_, file_system_context,
                     source_url_, destination_url_, space_needed),
      base::BindOnce(
          &FileManagerPrivateInternalStartCopyFunction::RunAfterStartCopy,
          this));
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterStartCopy(
    int operation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Respond(WithArguments(operation_id));
}

ExtensionFunction::ResponseAction FileManagerPrivateCancelCopyFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::CancelCopy::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  // We don't much take care about the result of cancellation.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CancelCopyOnIOThread, file_system_context,
                                params->copy_id));
  return RespondNow(WithArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalResolveIsolatedEntriesFunction::Run() {
  using extensions::api::file_manager_private_internal::ResolveIsolatedEntries::
      Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  DCHECK(file_system_context.get());

  const storage::ExternalFileSystemBackend* external_backend =
      file_system_context->external_backend();
  DCHECK(external_backend);

  file_manager::util::FileDefinitionList file_definition_list;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    const FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(GURL(params->urls[i]));
    DCHECK(external_backend->CanHandleType(file_system_url.type()))
        << "GURL: " << file_system_url.ToGURL()
        << "type: " << file_system_url.type();
    FileDefinition file_definition;
    const bool result =
        file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, source_url(), file_system_url.path(),
            &file_definition.virtual_path);
    if (!result) {
      LOG(WARNING) << "Failed to convert file_system_url to relative file "
                      "system path, type: "
                   << file_system_url.type();
      continue;
    }
    // The API only supports isolated files. It still works for directories,
    // as the value is ignored for existing entries.
    file_definition.is_directory = false;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      url::Origin::Create(source_url()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(
          &FileManagerPrivateInternalResolveIsolatedEntriesFunction::
              RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalResolveIsolatedEntriesFunction::
    RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  using extensions::api::file_manager_private_internal::EntryDescription;
  std::vector<EntryDescription> entries;

  for (const auto& definition : *entry_definition_list) {
    if (definition.error != base::File::FILE_OK) {
      LOG(WARNING) << "Error resolving file system URL: " << definition.error;
      continue;
    }
    EntryDescription entry;
    entry.file_system_name = definition.file_system_name;
    entry.file_system_root = definition.file_system_root_url;
    entry.file_full_path = "/" + definition.full_path.AsUTF8Unsafe();
    entry.file_is_directory = definition.is_directory;
    entries.push_back(std::move(entry));
  }

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           ResolveIsolatedEntries::Results::Create(entries)));
}

FileManagerPrivateInternalComputeChecksumFunction::
    FileManagerPrivateInternalComputeChecksumFunction()
    : digester_(base::MakeRefCounted<drive::util::FileStreamMd5Digester>()) {}

FileManagerPrivateInternalComputeChecksumFunction::
    ~FileManagerPrivateInternalComputeChecksumFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalComputeChecksumFunction::Run() {
  using drive::util::FileStreamMd5Digester;
  using extensions::api::file_manager_private_internal::ComputeChecksum::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty()) {
    return RespondNow(Error("File URL must be provided."));
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)));
  if (!file_system_url.is_valid()) {
    return RespondNow(Error("File URL was invalid"));
  }

  std::unique_ptr<storage::FileStreamReader> reader =
      file_system_context->CreateFileStreamReader(
          file_system_url, 0, storage::kMaximumLength, base::Time());

  FileStreamMd5Digester::ResultCallback result_callback = base::BindOnce(
      &ComputeChecksumRespondOnUIThread,
      base::BindOnce(
          &FileManagerPrivateInternalComputeChecksumFunction::RespondWith,
          this));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileStreamMd5Digester::GetMd5Digest, digester_,
                                std::move(reader), std::move(result_callback)));

  return RespondLater();
}

void FileManagerPrivateInternalComputeChecksumFunction::RespondWith(
    std::string hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Respond(WithArguments(std::move(hash)));
}

FileManagerPrivateSearchFilesByHashesFunction::
    FileManagerPrivateSearchFilesByHashesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateSearchFilesByHashesFunction::Run() {
  using api::file_manager_private::SearchFilesByHashes::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(hirono): Check the volume ID and fail the function for volumes other
  // than Drive.

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::EventLogger* const logger = file_manager::util::GetLogger(profile);
  if (logger) {
    logger->Log(logging::LOG_INFO,
                "%s[%d] called. (volume id: %s, number of hashes: %zd)", name(),
                request_id(), params->volume_id.c_str(),
                params->hash_list.size());
  }
  set_log_on_completion(true);

  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return RespondNow(Error("Drive not available"));
  }

  std::set<std::string> hashes(params->hash_list.begin(),
                               params->hash_list.end());

  // DriveFs doesn't provide dedicated backup solution yet, so for now just walk
  // the files and check MD5 extended attribute.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &FileManagerPrivateSearchFilesByHashesFunction::SearchByAttribute,
          this, hashes,
          integration_service->GetMountPointPath().Append(
              drive::util::kDriveMyDriveRootDirName),
          base::FilePath("/").Append(drive::util::kDriveMyDriveRootDirName)),
      base::BindOnce(
          &FileManagerPrivateSearchFilesByHashesFunction::OnSearchByAttribute,
          this, hashes));

  return RespondLater();
}

std::vector<drive::HashAndFilePath>
FileManagerPrivateSearchFilesByHashesFunction::SearchByAttribute(
    const std::set<std::string>& hashes,
    const base::FilePath& dir,
    const base::FilePath& prefix) {
  std::vector<drive::HashAndFilePath> results;

  if (hashes.empty())
    return results;

  std::set<std::string> remaining = hashes;
  std::vector<char> attribute;
  base::FileEnumerator enumerator(dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (GetFileExtendedAttribute(path, "user.drive.md5", &attribute)) {
      std::string md5(attribute.begin(), attribute.end());

      if (remaining.erase(md5)) {
        base::FilePath drive_path = prefix;
        bool success = dir.AppendRelativePath(path, &drive_path);
        DCHECK(success);
        results.push_back({md5, drive_path});
        if (remaining.empty())
          break;
      }
    }
  }

  return results;
}

void FileManagerPrivateSearchFilesByHashesFunction::OnSearchByAttribute(
    const std::set<std::string>& hashes,
    const std::vector<drive::HashAndFilePath>& results) {
  OnSearchByHashes(hashes, drive::FileError::FILE_ERROR_OK, results);
}

void FileManagerPrivateSearchFilesByHashesFunction::OnSearchByHashes(
    const std::set<std::string>& hashes,
    drive::FileError error,
    const std::vector<drive::HashAndFilePath>& search_results) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    Respond(Error(drive::FileErrorToString(error)));
    return;
  }

  base::Value::Dict result;
  for (const auto& hash : hashes) {
    result.EnsureList(hash);
  }

  for (const auto& hashAndPath : search_results) {
    base::Value::List* list = result.FindList(hashAndPath.hash);
    DCHECK(list);
    list->Append(hashAndPath.path.value());
  }
  Respond(WithArguments(std::move(result)));
}

FileManagerPrivateSearchFilesFunction::FileManagerPrivateSearchFilesFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateSearchFilesFunction::Run() {
  using api::file_manager_private::SearchFiles::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->search_params.max_results < 0) {
    return RespondNow(Error("maxResults must be non-negative"));
  }

  base::FilePath root = file_manager::util::GetMyFilesFolderForProfile(
      Profile::FromBrowserContext(browser_context()));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SearchByPattern, root, params->search_params.query,
                     base::internal::checked_cast<size_t>(
                         params->search_params.max_results)),
      base::BindOnce(&FileManagerPrivateSearchFilesFunction::OnSearchByPattern,
                     this));

  return RespondLater();
}

void FileManagerPrivateSearchFilesFunction::OnSearchByPattern(
    const std::vector<std::pair<base::FilePath, bool>>& results) {
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  auto my_files_path = file_manager::util::GetMyFilesFolderForProfile(profile);

  GURL url;
  base::FilePath my_files_virtual_path;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, my_files_path, source_url(), &url) ||
      !storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
          my_files_path, &my_files_virtual_path)) {
    Respond(Error("My files is not mounted"));
    return;
  }
  const std::string fs_name = my_files_virtual_path.value();
  const std::string fs_root = base::StrCat({url.spec(), "/"});

  base::Value::List entries;
  for (const auto& result : results) {
    base::FilePath fs_path("/");
    if (!my_files_path.AppendRelativePath(result.first, &fs_path)) {
      continue;
    }
    base::Value::Dict entry;
    entry.Set("fileSystemName", fs_name);
    entry.Set("fileSystemRoot", fs_root);
    entry.Set("fileFullPath", fs_path.AsUTF8Unsafe());
    entry.Set("fileIsDirectory", result.second);
    entries.Append(std::move(entry));
  }

  base::Value::Dict result;
  result.Set("entries", std::move(entries));
  Respond(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDirectorySizeFunction::Run() {
  using extensions::api::file_manager_private_internal::GetDirectorySize::
      Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty()) {
    return RespondNow(Error("File URL must be provided."));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)));
  if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url)) {
    return RespondNow(
        Error("FileSystemBackend failed to handle the entry's url."));
  }
  if (file_system_url.type() != storage::kFileSystemTypeLocal &&
      file_system_url.type() != storage::kFileSystemTypeDriveFs) {
    return RespondNow(Error("Only local directories are supported."));
  }

  const base::FilePath root_path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), profile, GURL(params->url));
  if (root_path.empty()) {
    return RespondNow(
        Error("Failed to get a local path from the entry's url."));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::ComputeDirectorySize, root_path),
      base::BindOnce(&FileManagerPrivateInternalGetDirectorySizeFunction::
                         OnDirectorySizeRetrieved,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalGetDirectorySizeFunction::
    OnDirectorySizeRetrieved(int64_t size) {
  Respond(WithArguments(static_cast<double>(size)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalStartIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::StartIOTask::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller())
    return RespondNow(Error("Cannot find VolumeManager"));

  std::vector<storage::FileSystemURL> source_urls;
  for (const std::string& url : params->urls) {
    storage::FileSystemURL cracked_url =
        file_system_context->CrackURLInFirstPartyContext(GURL(url));
    if (!cracked_url.is_valid()) {
      return RespondNow(Error("Invalid source URL *", Redact(url)));
    }
    source_urls.push_back(std::move(cracked_url));
  }

  auto type = IOTaskTypeToChromeEnum(params->type);
  if (!type) {
    return RespondNow(Error("Invalid I/O task type given."));
  }

  storage::FileSystemURL destination_folder_url;
  if (params->params.destination_folder_url) {
    destination_folder_url = file_system_context->CrackURLInFirstPartyContext(
        GURL(*(params->params.destination_folder_url)));
    if (!destination_folder_url.is_valid()) {
      return RespondNow(Error("Invalid destination folder url."));
    }
  }

  // Whether to display this IOTask notification, if undefined this should
  // default to true.
  bool show_notification = params->params.show_notification.value_or(true);

  // Check if Trash is enabled, this pref is mainly used by enterprise policy to
  // disable trash on a per profile basis.
  bool is_trash_enabled = false;
  if (base::FeatureList::IsEnabled(chromeos::features::kFilesTrash) &&
      profile && profile->GetPrefs()) {
    is_trash_enabled =
        profile->GetPrefs()->GetBoolean(ash::prefs::kFilesAppTrashEnabled);
  }

  std::unique_ptr<file_manager::io_task::IOTask> task;
  switch (type.value()) {
    case file_manager::io_task::OperationType::kCopy:
    case file_manager::io_task::OperationType::kMove:
      task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          type.value(), std::move(source_urls),
          std::move(destination_folder_url), profile, file_system_context,
          show_notification);
      break;
    case file_manager::io_task::OperationType::kZip:
      task = std::make_unique<file_manager::io_task::ZipIOTask>(
          std::move(source_urls), std::move(destination_folder_url), profile,
          file_system_context, show_notification);
      break;
    case file_manager::io_task::OperationType::kDelete:
      task = std::make_unique<file_manager::io_task::DeleteIOTask>(
          std::move(source_urls), file_system_context, show_notification);
      break;
    case file_manager::io_task::OperationType::kEmptyTrash:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::EmptyTrashIOTask>(
            blink::StorageKey(render_frame_host()->GetLastCommittedOrigin()),
            profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kRestore:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::RestoreIOTask>(
            std::move(source_urls), profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kRestoreToDestination:
      if (is_trash_enabled) {
        task =
            std::make_unique<file_manager::io_task::RestoreToDestinationIOTask>(
                std::move(source_urls), std::move(destination_folder_url),
                profile, file_system_context,
                /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kTrash:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::TrashIOTask>(
            std::move(source_urls), profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kExtract:
      if (base::FeatureList::IsEnabled(
              chromeos::features::kFilesExtractArchive)) {
        std::string password;
        if (params->params.password) {
          password = *params->params.password;
        }
        task = std::make_unique<file_manager::io_task::ExtractIOTask>(
            std::move(source_urls), std::move(password),
            std::move(destination_folder_url), profile, file_system_context,
            show_notification);
      }
      break;
  }
  if (!task) {
    return RespondNow(Error("Invalid operation type: *",
                            api::file_manager_private::ToString(params->type)));
  }
  const auto taskId =
      volume_manager->io_task_controller()->Add(std::move(task));
  return RespondNow(WithArguments(static_cast<int>(taskId)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateCancelIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::CancelIOTask::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller())
    return RespondNow(Error("Cannot find VolumeManager"));

  if (params->task_id <= 0) {
    return RespondNow(Error("Invalid task id"));
  }

  volume_manager->io_task_controller()->Cancel(params->task_id);
  return RespondNow(WithArguments());
}

FileManagerPrivateInternalParseTrashInfoFilesFunction::
    FileManagerPrivateInternalParseTrashInfoFilesFunction() = default;

FileManagerPrivateInternalParseTrashInfoFilesFunction::
    ~FileManagerPrivateInternalParseTrashInfoFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalParseTrashInfoFilesFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::ParseTrashInfoFiles::
      Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* const profile = Profile::FromBrowserContext(browser_context());
  file_system_context_ =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  std::vector<base::FilePath> trash_info_paths;
  for (const std::string& url : params->urls) {
    storage::FileSystemURL cracked_url =
        file_system_context_->CrackURLInFirstPartyContext(GURL(url));
    if (!cracked_url.is_valid()) {
      return RespondNow(Error("Invalid source url."));
    }
    trash_info_paths.push_back(cracked_url.path());
  }

  validator_ = std::make_unique<file_manager::trash::TrashInfoValidator>(
      profile, /*base_path=*/base::FilePath());

  auto barrier_callback = base::BarrierCallback<
      base::FileErrorOr<file_manager::trash::ParsedTrashInfoData>>(
      trash_info_paths.size(),
      base::BindOnce(&FileManagerPrivateInternalParseTrashInfoFilesFunction::
                         OnTrashInfoFilesParsed,
                     this));

  for (const base::FilePath& path : trash_info_paths) {
    validator_->ValidateAndParseTrashInfo(std::move(path), barrier_callback);
  }

  return RespondLater();
}

void FileManagerPrivateInternalParseTrashInfoFilesFunction::
    OnTrashInfoFilesParsed(
        std::vector<base::FileErrorOr<file_manager::trash::ParsedTrashInfoData>>
            parsed_data) {
  // The underlying trash service could potentially live longer than the Files
  // window that invoked this function, ensure the frame host and browser
  // context are alive before continuing.
  if (!render_frame_host() || !browser_context()) {
    LOG(WARNING) << "Parsing trashinfo files finished but no window available "
                    "to respond to";
    Respond(WithArguments());
    return;
  }

  file_manager::util::FileDefinitionList file_definition_list;
  std::vector<file_manager::trash::ParsedTrashInfoData> valid_data;
  url::Origin origin = render_frame_host()->GetLastCommittedOrigin();

  for (auto& trash_info_data : parsed_data) {
    if (!trash_info_data.has_value()) {
      LOG(ERROR) << "Failed parsing trashinfo file: "
                 << trash_info_data.error();
      continue;
    }

    file_manager::util::FileDefinition file_definition;
    if (!file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            Profile::FromBrowserContext(browser_context()), origin.GetURL(),
            trash_info_data.value().absolute_restore_path,
            &file_definition.virtual_path)) {
      LOG(ERROR) << "Failed to convert absolute path to relative path";
      continue;
    }

    file_definition_list.push_back(std::move(file_definition));
    valid_data.push_back(std::move(trash_info_data.value()));
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_system_context_, origin, std::move(file_definition_list),
      base::BindOnce(&FileManagerPrivateInternalParseTrashInfoFilesFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this, std::move(valid_data)));
}

void FileManagerPrivateInternalParseTrashInfoFilesFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::vector<file_manager::trash::ParsedTrashInfoData> parsed_data,
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK_EQ(parsed_data.size(), entry_definition_list->size());
  std::vector<api::file_manager_private_internal::ParsedTrashInfoFile> results;

  for (size_t i = 0; i < parsed_data.size(); ++i) {
    const auto& [trash_info_path, trashed_file_path, absolute_restore_path,
                 deletion_date] = parsed_data[i];
    api::file_manager_private_internal::ParsedTrashInfoFile info;

    info.restore_entry.file_system_name =
        entry_definition_list->at(i).file_system_name;
    info.restore_entry.file_system_root =
        entry_definition_list->at(i).file_system_root_url;
    info.restore_entry.file_full_path =
        base::FilePath("/")
            .Append(entry_definition_list->at(i).full_path)
            .value();
    info.restore_entry.file_is_directory =
        entry_definition_list->at(i).is_directory;
    info.trash_info_file_name = trash_info_path.BaseName().value();
    info.deletion_date = deletion_date.ToJsTimeIgnoringNull();

    results.push_back(std::move(info));
  }

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           ParseTrashInfoFiles::Results::Create(results)));
}

}  // namespace extensions
