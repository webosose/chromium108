// Copyright 2020 LG Electronics, Inc.
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

#include "components/local_storage_tracker/common/local_storage_tracker_store.h"

#include "base/bind.h"
#include "base/logging.h"

namespace content {

LocalStorageTrackerStore::LocalStorageTrackerStore(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner)
    : main_thread_runner_(main_thread_runner),
      db_thread_runner_(db_thread_runner),
      db_initialized_(false) {
  DCHECK(main_thread_runner != nullptr);
  DCHECK(db_thread_runner != nullptr);
}

void LocalStorageTrackerStore::Initialize(
    const base::FilePath& data_file_path,
    base::OnceCallback<void(bool)> callback) {
  VLOG(1) << "##### Store initialization; data path="
          << data_file_path.AsUTF8Unsafe();
  db_.reset(new LocalStorageTrackerDatabase(data_file_path));
  RunOnDBThread(base::BindOnce(&LocalStorageTrackerStore::InitializeOnDBThread,
                               base::Unretained(this), std::move(callback)));
}

void LocalStorageTrackerStore::AddAccess(
    const AccessData& access,
    base::OnceCallback<void(bool)> callback) {
  RunOnDBThread(base::BindOnce(&LocalStorageTrackerStore::AddAccessOnDBThread,
                               base::Unretained(this), access,
                               std::move(callback)));
}

void LocalStorageTrackerStore::AddApplication(
    const ApplicationData& application,
    base::OnceCallback<void(bool)> callback) {
  RunOnDBThread(
      base::BindOnce(&LocalStorageTrackerStore::AddApplicationOnDBThread,
                     base::Unretained(this), application, std::move(callback)));
}

void LocalStorageTrackerStore::AddOrigin(
    const OriginData& origin,
    base::OnceCallback<void(bool)> callback) {
  RunOnDBThread(base::BindOnce(&LocalStorageTrackerStore::AddOriginOnDBThread,
                               base::Unretained(this), origin,
                               std::move(callback)));
}

void LocalStorageTrackerStore::DeleteApplication(
    const std::string& app_id,
    base::OnceCallback<void(bool)> callback) {
  RunOnDBThread(
      base::BindOnce(&LocalStorageTrackerStore::DeleteApplicationOnDBThread,
                     base::Unretained(this), app_id, std::move(callback)));
}

void LocalStorageTrackerStore::DeleteOrigin(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  RunOnDBThread(
      base::BindOnce(&LocalStorageTrackerStore::DeleteOriginOnDBThread,
                     base::Unretained(this), url, std::move(callback)));
}

void LocalStorageTrackerStore::GetAccesses(
    base::OnceCallback<void(bool, const AccessDataList&)> callback) {
  RunOnDBThread(base::BindOnce(&LocalStorageTrackerStore::GetAccessesOnDBThread,
                               base::Unretained(this), std::move(callback)));
}

void LocalStorageTrackerStore::GetApplications(
    base::OnceCallback<void(bool, const ApplicationDataList&)> callback) {
  RunOnDBThread(
      base::BindOnce(&LocalStorageTrackerStore::GetApplicationsOnDBThread,
                     base::Unretained(this), std::move(callback)));
}

void LocalStorageTrackerStore::InitializeOnDBThread(
    base::OnceCallback<void(bool)> callback) {
  db_initialized_ = db_->Init() == sql::INIT_OK;
  main_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), db_initialized_));
}

void LocalStorageTrackerStore::AddAccessOnDBThread(
    const AccessData& access,
    base::OnceCallback<void(bool)> callback) {
  bool result = false;
  if (db_initialized_) {
    result = db_->AddAccess(access);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result));
}

void LocalStorageTrackerStore::AddApplicationOnDBThread(
    const ApplicationData& application,
    base::OnceCallback<void(bool)> callback) {
  bool result = false;
  if (db_initialized_) {
    result = db_->AddApplication(application);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result));
}

void LocalStorageTrackerStore::AddOriginOnDBThread(
    const OriginData& origin,
    base::OnceCallback<void(bool)> callback) {
  bool result = false;
  if (db_initialized_) {
    result = db_->AddOrigin(origin);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result));
}

void LocalStorageTrackerStore::DeleteApplicationOnDBThread(
    const std::string& app_id,
    base::OnceCallback<void(bool)> callback) {
  bool result = false;
  if (db_initialized_) {
    result = db_->DeleteApplication(app_id);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result));
}

void LocalStorageTrackerStore::DeleteOriginOnDBThread(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  bool result = false;
  if (db_initialized_) {
    result = db_->DeleteOrigin(url);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result));
}

void LocalStorageTrackerStore::GetAccessesOnDBThread(
    base::OnceCallback<void(bool, const AccessDataList&)> callback) {
  AccessDataList data_list;
  bool result = false;
  if (db_initialized_) {
    result = db_->GetAccesses(&data_list);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result, data_list));
}

void LocalStorageTrackerStore::GetApplicationsOnDBThread(
    base::OnceCallback<void(bool, const ApplicationDataList&)> callback) {
  ApplicationDataList data_list;
  bool result = false;
  if (db_initialized_) {
    result = db_->GetApplications(&data_list);
  }
  RunOnUIThread(base::BindOnce(std::move(callback), result, data_list));
}

void LocalStorageTrackerStore::RunOnDBThread(base::OnceClosure task) {
  db_thread_runner_->PostTask(FROM_HERE, std::move(task));
}

void LocalStorageTrackerStore::RunOnUIThread(base::OnceClosure task) {
  main_thread_runner_->PostTask(FROM_HERE, std::move(task));
}

}  // namespace content
