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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_DATABASE_WEBRISK_SQLITE_DATABASE_STORE_H_
#define NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_DATABASE_WEBRISK_SQLITE_DATABASE_STORE_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "neva/browser_service/browser/webrisk/core/database/webrisk_database.h"
#include "neva/browser_service/browser/webrisk/core/webrisk.pb.h"
#include "neva/browser_service/browser/webrisk/core/webrisk_data_store.h"

namespace webrisk {

class WebRiskSQLiteDatabaseStore : public webrisk::WebRiskDataStore {
 public:
  WebRiskSQLiteDatabaseStore() = default;
  ~WebRiskSQLiteDatabaseStore() override = default;

  static scoped_refptr<WebRiskSQLiteDatabaseStore> Create();

  WebRiskSQLiteDatabaseStore(const WebRiskSQLiteDatabaseStore&) = delete;
  WebRiskSQLiteDatabaseStore& operator=(const WebRiskSQLiteDatabaseStore&) =
      delete;

  bool Initialize() override;
  bool InsertThreatEntries(const std::string& raw_hashes);
  bool DeleteThreatEntries(const std::vector<int>& removals);
  bool InsertThreatEntry(const std::string& hash_prefix);
  bool DeleteThreatEntry(const std::string& hash_prefix);
  bool DeleteAllEntries();

  bool WriteDataToDisk(
      const ComputeThreatListDiffResponse& file_format) override;
  bool IsHashPrefixExpired() override;
  bool IsHashPrefixAvailable(const std::string& hash_prefix) override;
  bool MigrateDataFromLocalFile() override;
  bool InsertOrUpdateVersionToken(const std::string& version_token,
                                  bool is_insert_token) override;
  std::string GetVersionToken() override;

 private:
  std::vector<WebriskThreatEntry> CreateEntryListFromRawHashes(
      const std::string& raw_hashes);
  std::vector<int> CreateRemovalEntryList(
      const ComputeThreatListDiffResponse& file_format);

  SEQUENCE_CHECKER(sequence_checker_);
  webrisk::WebRiskDatabase webrisk_db_;
  bool is_initialized_ = false;
};

}  // namespace webrisk

#endif  // NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_DATABASE_WEBRISK_SQLITE_DATABASE_STORE_H_