// Copyright 2023 LG Electronics, Inc.
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

#include "neva/browser_service/browser/webrisk/core/database/webrisk_sqlite_database_store.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "neva/browser_service/browser/webrisk/core/database/webrisk_database.h"
#include "neva/browser_service/browser/webrisk/core/webrisk_utils.h"

namespace webrisk {

scoped_refptr<WebRiskDataStore> WebRiskDataStore::Create() {
  return base::MakeRefCounted<WebRiskSQLiteDatabaseStore>();
}

bool WebRiskSQLiteDatabaseStore::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_initialized_)
    return true;
  is_initialized_ = webrisk_db_.Init();
  if (!is_initialized_) {
    webrisk_db_.CloseDatabase();
  }
  return is_initialized_;
}

bool WebRiskSQLiteDatabaseStore::InsertThreatEntry(
    const std::string& hash_prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  WebriskThreatEntry entry;
  entry.hash_prefix = hash_prefix;
  return webrisk_db_.InsertThreatEntry(entry);
}

bool WebRiskSQLiteDatabaseStore::DeleteThreatEntry(
    const std::string& hash_prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  WebriskThreatEntry entry;
  entry.hash_prefix = hash_prefix;
  return webrisk_db_.DeleteThreatEntry(entry);
}

bool WebRiskSQLiteDatabaseStore::DeleteAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  return webrisk_db_.DeleteAllEntries();
}

bool WebRiskSQLiteDatabaseStore::WriteDataToDisk(
    const ComputeThreatListDiffResponse& file_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  DeleteAllEntries();
  std::string raw_hashes;
  raw_hashes = file_format.additions().raw_hashes(0).raw_hashes();
  InsertThreatEntries(raw_hashes);
  return true;
}

void WebRiskSQLiteDatabaseStore::InsertThreatEntries(
    const std::string& raw_hashes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return;
  }
  std::vector<WebriskThreatEntry> entry_list;
  CreateEntryListFromRawHashes(raw_hashes, entry_list);
  webrisk_db_.InsertThreatEntries(entry_list);
}

void WebRiskSQLiteDatabaseStore::CreateEntryListFromRawHashes(
    const std::string& raw_hashes,
    std::vector<WebriskThreatEntry>& entry_list) {
  std::string hash_list;
  base::Base64Decode(raw_hashes, &hash_list);
  for (int i = 0; i < hash_list.length(); i += kHashPrefixSize) {
    std::string hash_prefix = hash_list.substr(i, kHashPrefixSize);
    WebriskThreatEntry entry;
    entry.hash_prefix = hash_prefix;
    entry_list.push_back(entry);
  }
}

bool WebRiskSQLiteDatabaseStore::IsHashPrefixExpired() {
  return (update_time_ > base::TimeDelta());
}

bool WebRiskSQLiteDatabaseStore::IsHashPrefixAvailable(
    const std::string& hash_prefix) {
  if (!is_initialized_) {
    return false;
  }
  return webrisk_db_.IsHashPrefixAvailable(hash_prefix);
}

bool WebRiskSQLiteDatabaseStore::MigrateDataFromLocalFile() {
  const base::FilePath file_path = GetFilePath(kWebRiskStoreFileName);
  if (base::PathExists(file_path)) {
    std::string compute_diff;
    if (!base::ReadFileToStringWithMaxSize(file_path, &compute_diff,
                                           kMaxWebRiskStoreSize)) {
      return false;
    }

    if (compute_diff.empty())
      return false;

    ComputeThreatListDiffResponse file_format;
    if (!file_format.ParseFromString(compute_diff))
      return false;

    bool is_data_migrated = WriteDataToDisk(file_format);
    update_time_ = GetNextUpdateTime(file_format.recommended_next_diff());
    is_data_migrated &= base::DeleteFile(file_path);
    return is_data_migrated;
  }
  return false;
}

}  // namespace webrisk