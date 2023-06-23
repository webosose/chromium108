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

#ifndef NEVA_BROWSER_SERVICE_WEBRISK_CORE_DATABASE_WEBRISK_DATABASE_H_
#define NEVA_BROWSER_SERVICE_WEBRISK_CORE_DATABASE_WEBRISK_DATABASE_H_

#include <string>
#include <vector>

#include "sql/database.h"

namespace base {

class FilePath;

}

namespace webrisk {

// This struct contain details for each threat entry reponse from fetch hashes
struct WebriskThreatEntry {
  WebriskThreatEntry() = default;
  WebriskThreatEntry(const WebriskThreatEntry& other) = default;
  ~WebriskThreatEntry() = default;

  std::string hash_prefix;
};

// Implements communication interface with sqlite database
class WebRiskDatabase {
 public:
  WebRiskDatabase();
  ~WebRiskDatabase() = default;

  bool Init();
  bool InsertThreatEntry(const WebriskThreatEntry& entry);
  bool InsertThreatEntries(const std::vector<WebriskThreatEntry>& entry_list);
  bool DeleteThreatEntry(const WebriskThreatEntry& entry);
  bool DeleteThreatEntries(const std::vector<WebriskThreatEntry>& entry_list);
  bool DeleteAllEntries();
  bool IsHashPrefixAvailable(const std::string& hash_prefix);
  void CloseDatabase();

 private:
  bool CreateTableIfNeeded();
  WebRiskDatabase(const WebRiskDatabase&) = delete;
  WebRiskDatabase& operator=(const WebRiskDatabase&) = delete;

  sql::Database db_;
  base::FilePath db_file_path_;
};

}  // namespace webrisk

#endif  // NEVA_BROWSER_SERVICE_WEBRISK_CORE_DATABASE_WEBRISK_DATABASE_H_