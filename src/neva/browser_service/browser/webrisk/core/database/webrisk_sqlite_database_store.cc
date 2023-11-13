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
  bool is_write_data_succeed = false;

  // Removals entries
  if (!(file_format.response_type() == ComputeThreatListDiffResponse::RESET)) {
    std::vector<int> removals = CreateRemovalEntryList(file_format);
    is_write_data_succeed &= DeleteThreatEntries(removals);
  } else {
    is_write_data_succeed &= DeleteAllEntries();
  }

  // Addition entries
  std::string raw_hashes;
  raw_hashes = file_format.additions().raw_hashes(0).raw_hashes();
  is_write_data_succeed &= InsertThreatEntries(raw_hashes);

  // Insert/Update new version token
  std::string new_version_token = file_format.new_version_token();
  std::string old_version_token = GetVersionToken();
  is_write_data_succeed &=
      InsertOrUpdateVersionToken(new_version_token, old_version_token.empty());

  return is_write_data_succeed;
}

bool WebRiskSQLiteDatabaseStore::InsertThreatEntries(
    const std::string& raw_hashes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  std::vector<WebriskThreatEntry> entries =
      CreateEntryListFromRawHashes(raw_hashes);
  return webrisk_db_.InsertThreatEntries(entries);
}

bool WebRiskSQLiteDatabaseStore::DeleteThreatEntries(
    const std::vector<int>& removals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  return webrisk_db_.DeleteThreatEntries(removals);
}

std::vector<WebriskThreatEntry>
WebRiskSQLiteDatabaseStore::CreateEntryListFromRawHashes(
    const std::string& raw_hashes) {
  std::string hash_list;
  std::vector<WebriskThreatEntry> entries;
  base::Base64Decode(raw_hashes, &hash_list);
  for (int i = 0; i < hash_list.length(); i += kHashPrefixSize) {
    std::string hash_prefix = hash_list.substr(i, kHashPrefixSize);
    WebriskThreatEntry entry;
    entry.hash_prefix = hash_prefix;
    entries.push_back(entry);
  }
  return entries;
}

std::vector<int> WebRiskSQLiteDatabaseStore::CreateRemovalEntryList(
    const ComputeThreatListDiffResponse& file_format) {
  std::vector<int> removals;
  int indices_size = file_format.removals().raw_indices().indices_size();
  for (int i = 0; i < indices_size; i++) {
    int idx = file_format.removals().raw_indices().indices(i);
    removals.push_back(idx);
  }
  return removals;
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

bool WebRiskSQLiteDatabaseStore::InsertOrUpdateVersionToken(
    const std::string& version_token,
    bool is_insert_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized_) {
    return false;
  }
  return webrisk_db_.InsertOrUpdateVersionToken(version_token, is_insert_token);
}

std::string WebRiskSQLiteDatabaseStore::GetVersionToken() {
  if (!is_initialized_) {
    return std::string();
  }
  return webrisk_db_.GetVersionToken();
}

}  // namespace webrisk