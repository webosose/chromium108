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

#include "neva/browser_service/browser/webrisk/core/database/webrisk_database.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "neva/browser_service/browser/webrisk/core/webrisk_utils.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

const char kDatabaseFileName[] = "webrisk.db";
const char kWebRiskHashPrefixTableName[] = "hash_prefix";

}  // namespace

namespace webrisk {

WebRiskDatabase::WebRiskDatabase() : db_({.cache_size = 8}) {}

bool WebRiskDatabase::Init() {
  db_file_path_ = GetFilePath(kDatabaseFileName);
  if (!db_.Open(db_file_path_)) {
    LOG(ERROR) << __func__ << " WebRiskDatabase open operation failed";
    return false;
  }
  if (!CreateTableIfNeeded()) {
    LOG(ERROR) << __func__ << " Creation of table failed";
    return false;
  }
  return true;
}

bool WebRiskDatabase::InsertThreatEntry(const WebriskThreatEntry& entry) {
  const std::string query = base::StringPrintf(
      "INSERT INTO %s (prefix) "
      "SELECT ? "
      "WHERE NOT EXISTS "
      "(SELECT prefix "
      " FROM %s "
      " WHERE prefix = ?)",
      kWebRiskHashPrefixTableName, kWebRiskHashPrefixTableName);
  sql::Statement insert_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, query.c_str()));
  insert_statement.BindString(0, entry.hash_prefix);
  insert_statement.BindString(1, entry.hash_prefix);
  return insert_statement.Run();
}

bool WebRiskDatabase::InsertThreatEntries(
    const std::vector<WebriskThreatEntry>& entry_list) {
  if (!db_.BeginTransaction()) {
    LOG(ERROR) << __func__ << " Failed to begin the transaction.";
    return false;
  }
  for (const auto& entry : entry_list) {
    InsertThreatEntry(entry);
    int last_errno = db_.GetLastErrno();
    if (last_errno != SQLITE_OK) {
      LOG(ERROR) << __func__ << " Insertion in DB failed";
      db_.RollbackTransaction();
      return false;
    }
  }

  return db_.CommitTransaction();
}

bool WebRiskDatabase::DeleteThreatEntry(const WebriskThreatEntry& entry) {
  const std::string query = base::StringPrintf(
      "DELETE FROM %s WHERE prefix = ?", kWebRiskHashPrefixTableName);
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, query.c_str()));
  delete_statement.BindString(0, entry.hash_prefix);

  return (delete_statement.Run() && db_.GetLastChangeCount());
}

bool WebRiskDatabase::DeleteThreatEntries(
    const std::vector<WebriskThreatEntry>& entry_list) {
  if (!db_.BeginTransaction()) {
    LOG(ERROR) << __func__ << " Failed to begin the transaction.";
    return false;
  }
  for (const auto& entry : entry_list) {
    DeleteThreatEntry(entry);
    int last_errno = db_.GetLastErrno();
    if (last_errno != SQLITE_OK) {
      LOG(ERROR) << __func__ << " Insertion in DB failed";
      db_.RollbackTransaction();
      return false;
    }
  }

  return db_.CommitTransaction();
}

bool WebRiskDatabase::DeleteAllEntries() {
  const std::string query =
      base::StringPrintf("DELETE FROM %s", kWebRiskHashPrefixTableName);
  sql::Statement delete_all_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, query.c_str()));
  return (delete_all_statement.Run() && db_.GetLastChangeCount());
}

bool WebRiskDatabase::IsHashPrefixAvailable(const std::string& hash_prefix) {
  int count = 0;
  const std::string query =
      base::StringPrintf("SELECT COUNT(*) FROM %s WHERE prefix LIKE ?",
                         kWebRiskHashPrefixTableName);
  sql::Statement response_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, query.c_str()));
  response_statement.BindString(0, hash_prefix);
  if (response_statement.Step())
    count = response_statement.ColumnInt(0);

  return (count > 0);
}

bool WebRiskDatabase::CreateTableIfNeeded() {
  if (db_.DoesTableExist(kWebRiskHashPrefixTableName)) {
    LOG(INFO) << __func__ << " Table already exists";
    return true;
  }

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s ( "
      "prefix TEXT PRIMARY KEY NOT NULL"
      ")",
      kWebRiskHashPrefixTableName);

  if (!db_.Execute(query.c_str())) {
    LOG(ERROR) << __func__ << " Error creating " << kWebRiskHashPrefixTableName
               << " table";
    return false;
  }

  return true;
}

void WebRiskDatabase::CloseDatabase() {
  DCHECK(db_.is_open());
  db_.Close();
}

}  // namespace webrisk