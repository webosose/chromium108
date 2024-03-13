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

#ifndef COMPONENTS_LOCAL_STORAGE_TRACKER_COMMON_LOCAL_STORAGE_TRACKER_STORE_H_
#define COMPONENTS_LOCAL_STORAGE_TRACKER_COMMON_LOCAL_STORAGE_TRACKER_STORE_H_

#include <memory>

#include "base/threading/thread.h"
#include "components/local_storage_tracker/common/local_storage_tracker_database.h"

namespace content {

class LocalStorageTrackerStore {
 public:
  LocalStorageTrackerStore(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SequencedTaskRunner> db_thread_runner);
  void Initialize(const base::FilePath& data_file_path,
                  base::OnceCallback<void(bool)> callback);
  void AddAccess(const AccessData& access,
                 base::OnceCallback<void(bool)> callback);
  void AddApplication(const ApplicationData& application,
                      base::OnceCallback<void(bool)> callback);
  void AddOrigin(const OriginData& origin,
                 base::OnceCallback<void(bool)> callback);
  void DeleteApplication(const std::string& app_id,
                         base::OnceCallback<void(bool)> callback);
  void DeleteOrigin(const GURL& url, base::OnceCallback<void(bool)> callback);
  void GetAccesses(
      base::OnceCallback<void(bool, const AccessDataList&)> callback);
  void GetApplications(
      base::OnceCallback<void(bool, const ApplicationDataList&)> callback);

 private:
  void InitializeOnDBThread(base::OnceCallback<void(bool)> callback);
  void AddAccessOnDBThread(const AccessData& access,
                           base::OnceCallback<void(bool)> callback);
  void AddApplicationOnDBThread(const ApplicationData& application,
                                base::OnceCallback<void(bool)> callback);
  void AddOriginOnDBThread(const OriginData& origin,
                           base::OnceCallback<void(bool)> callback);
  void DeleteApplicationOnDBThread(const std::string& app_id,
                                   base::OnceCallback<void(bool)> callback);
  void DeleteOriginOnDBThread(const GURL& url,
                              base::OnceCallback<void(bool)> callback);
  void GetAccessesOnDBThread(
      base::OnceCallback<void(bool, const AccessDataList&)> callback);
  void GetApplicationsOnDBThread(
      base::OnceCallback<void(bool, const ApplicationDataList&)> callback);

  void RunOnDBThread(base::OnceClosure task);
  void RunOnUIThread(base::OnceClosure task);

  std::unique_ptr<LocalStorageTrackerDatabase> db_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
  scoped_refptr<base::SequencedTaskRunner> db_thread_runner_;
  bool db_initialized_;
  static const std::string db_name_;
};

}  // namespace content

#endif  // COMPONENTS_LOCAL_STORAGE_TRACKER_COMMON_LOCAL_STORAGE_TRACKER_STORE_H_
