// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_launch_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/origin.h"

namespace web_app {

namespace {

// On Chrome OS paths that exist on an external mount point need to be treated
// differently to make sure the File System Access code accesses these paths via
// the correct file system backend. This method checks if this is the case, and
// updates `entry_path` to the path that should be used by the File System
// Access implementation.
content::FileSystemAccessEntryFactory::PathType MaybeRemapPath(
    base::FilePath* entry_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath virtual_path;
  auto* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (external_mount_points->GetVirtualPath(*entry_path, &virtual_path)) {
    *entry_path = std::move(virtual_path);
    return content::FileSystemAccessEntryFactory::PathType::kExternal;
  }
#endif
  return content::FileSystemAccessEntryFactory::PathType::kLocal;
}

class EntriesBuilder {
 public:
  EntriesBuilder(content::WebContents* web_contents,
                 const GURL& launch_url,
                 size_t expected_number_of_entries)
      : entry_factory_(web_contents->GetPrimaryMainFrame()
                           ->GetProcess()
                           ->GetStoragePartition()
                           ->GetFileSystemAccessEntryFactory()),
        context_(blink::StorageKey(url::Origin::Create(launch_url)),
                 launch_url,
                 content::GlobalRenderFrameHostId(
                     web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
                     web_contents->GetPrimaryMainFrame()->GetRoutingID())) {
    entries_.reserve(expected_number_of_entries);
  }

  void AddFileEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateFileEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kSave));
  }

  void AddDirectoryEntry(const base::FilePath& path) {
    base::FilePath entry_path = path;
    content::FileSystemAccessEntryFactory::PathType path_type =
        MaybeRemapPath(&entry_path);
    entries_.push_back(entry_factory_->CreateDirectoryEntryFromPath(
        context_, path_type, entry_path,
        content::FileSystemAccessEntryFactory::UserAction::kOpen));
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> Build() {
    return std::move(entries_);
  }

 private:
  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries_;
  scoped_refptr<content::FileSystemAccessEntryFactory> entry_factory_;
  content::FileSystemAccessEntryFactory::BindingContext context_;
};

}  // namespace

WebAppLaunchQueue::WebAppLaunchQueue(content::WebContents* web_contents,
                                     const WebAppRegistrar& registrar)
    : content::WebContentsObserver(web_contents), registrar_(registrar) {}

WebAppLaunchQueue::~WebAppLaunchQueue() = default;

void WebAppLaunchQueue::Enqueue(WebAppLaunchParams launch_params) {
  DCHECK(registrar_.IsUrlInAppScope(launch_params.target_url,
                                    launch_params.app_id));
  DCHECK(launch_params.dir.empty() ||
         registrar_.IsSystemApp(launch_params.app_id));

  // Drop the existing queue state if a new launch navigation was started.
  if (launch_params.started_new_navigation) {
    Reset();
    queue_.push_back(std::move(launch_params));
    pending_navigation_ = true;
    return;
  }

  if (!queue_.empty())
    DCHECK_EQ(launch_params.app_id, queue_.front().app_id);
  queue_.push_back(std::move(launch_params));
  if (!pending_navigation_)
    SendQueuedLaunchParams(web_contents()->GetLastCommittedURL());
}

void WebAppLaunchQueue::Reset() {
  queue_.clear();
  pending_navigation_ = false;
  last_sent_queued_launch_params_.reset();
}

const AppId* WebAppLaunchQueue::GetPendingLaunchAppId() const {
  if (queue_.empty())
    return nullptr;
  return &(queue_.front().app_id);
}

void WebAppLaunchQueue::DidFinishNavigation(content::NavigationHandle* handle) {
  // Currently, launch data is only sent the primary main frame.
  if (!handle->IsInPrimaryMainFrame())
    return;

  if (pending_navigation_) {
    pending_navigation_ = false;
    // The launch navigation may have redirected out of the app scope.
    if (!registrar_.IsUrlInAppScope(handle->GetURL(), queue_.front().app_id)) {
      Reset();
      return;
    }

    SendQueuedLaunchParams(handle->GetURL());
    return;
  }

  // Reloads have the last sent launch params re-sent as they may contain live
  // file handles that should persist across reloads.
  if (last_sent_queued_launch_params_ &&
      handle->GetReloadType() != content::ReloadType::NONE) {
    SendLaunchParams(*last_sent_queued_launch_params_, handle->GetURL());
    return;
  }

  // Leaving the document resets all queue state.
  if (!handle->IsSameDocument())
    Reset();
}

void WebAppLaunchQueue::SendQueuedLaunchParams(const GURL& current_url) {
  for (WebAppLaunchParams& launch_params : queue_) {
    if (&launch_params == &queue_.back())
      last_sent_queued_launch_params_ = launch_params;
    SendLaunchParams(std::move(launch_params), current_url);
  }
  queue_.clear();
}

void WebAppLaunchQueue::SendLaunchParams(WebAppLaunchParams launch_params,
                                         const GURL& current_url) {
  DCHECK(registrar_.IsUrlInAppScope(current_url, launch_params.app_id));
  mojo::AssociatedRemote<blink::mojom::WebLaunchService> launch_service;
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&launch_service);
  DCHECK(launch_service);

  if (!launch_params.paths.empty() || !launch_params.dir.empty()) {
    EntriesBuilder entries_builder(web_contents(), launch_params.target_url,
                                   launch_params.paths.size() + 1);
    if (!launch_params.dir.empty())
      entries_builder.AddDirectoryEntry(launch_params.dir);

    for (const auto& path : launch_params.paths)
      entries_builder.AddFileEntry(path);

    launch_service->SetLaunchFiles(entries_builder.Build());
  } else {
    launch_service->EnqueueLaunchParams(launch_params.target_url);
  }
}

}  // namespace web_app
