// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database.h"

#include <inttypes.h>

#include <algorithm>
#include <climits>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

// Because each entry is a key-value pair, and both keys and values are
// std::u16strings and bounded by `max_string_length_`, the total bytes used per
// entry is at most 2 * 2 * `max_string_length_`.
const int kSharedStorageEntryTotalBytesMultiplier = 4;

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

[[nodiscard]] std::string SerializeOrigin(const url::Origin& origin) {
  DCHECK(!origin.opaque());
  DCHECK_NE(url::kFileScheme, origin.scheme());
  return origin.Serialize();
}

[[nodiscard]] bool InitSchema(sql::Database& db) {
  static constexpr char kValuesMappingSql[] =
      "CREATE TABLE IF NOT EXISTS values_mapping("
      "context_origin TEXT NOT NULL,"
      "key TEXT NOT NULL,"
      "value TEXT,"
      "PRIMARY KEY(context_origin,key)) WITHOUT ROWID";
  if (!db.Execute(kValuesMappingSql))
    return false;

  // Note: In `per_origin_mapping`, `last_used_time` is now the origin's most
  // recent creation time. We are keeping the outdated name while changing the
  // de facto usage of the data field in order to avoid an expensive database
  // migration.
  //
  // TODO(cammie): If we ever alter this schema in the future, change the name
  // of `last_used_time` to `creation_time`.
  static constexpr char kPerOriginMappingSql[] =
      "CREATE TABLE IF NOT EXISTS per_origin_mapping("
      "context_origin TEXT NOT NULL PRIMARY KEY,"
      "last_used_time INTEGER NOT NULL,"
      "length INTEGER NOT NULL) WITHOUT ROWID";
  if (!db.Execute(kPerOriginMappingSql))
    return false;

  static constexpr char kBudgetMappingSql[] =
      "CREATE TABLE IF NOT EXISTS budget_mapping("
      "id INTEGER NOT NULL PRIMARY KEY,"
      "context_origin TEXT NOT NULL,"
      "time_stamp INTEGER NOT NULL,"
      "bits_debit REAL NOT NULL)";
  if (!db.Execute(kBudgetMappingSql))
    return false;

  static constexpr char kLastUsedTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS per_origin_mapping_last_used_time_idx "
      "ON per_origin_mapping(last_used_time)";
  if (!db.Execute(kLastUsedTimeIndexSql))
    return false;

  static constexpr char kOriginTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS budget_mapping_origin_time_stamp_idx "
      "ON budget_mapping(context_origin,time_stamp)";
  if (!db.Execute(kOriginTimeIndexSql))
    return false;

  return true;
}

}  // namespace

SharedStorageDatabase::GetResult::GetResult() = default;

SharedStorageDatabase::GetResult::GetResult(GetResult&&) = default;

SharedStorageDatabase::GetResult::GetResult(OperationResult result)
    : result(result) {}

SharedStorageDatabase::GetResult::GetResult(std::u16string data,
                                            OperationResult result)
    : data(data), result(result) {}

SharedStorageDatabase::GetResult::~GetResult() = default;

SharedStorageDatabase::GetResult& SharedStorageDatabase::GetResult::operator=(
    GetResult&&) = default;

SharedStorageDatabase::BudgetResult::BudgetResult(BudgetResult&&) = default;

SharedStorageDatabase::BudgetResult::BudgetResult(double bits,
                                                  OperationResult result)
    : bits(bits), result(result) {}

SharedStorageDatabase::BudgetResult::~BudgetResult() = default;

SharedStorageDatabase::BudgetResult&
SharedStorageDatabase::BudgetResult::operator=(BudgetResult&&) = default;

SharedStorageDatabase::TimeResult::TimeResult() = default;

SharedStorageDatabase::TimeResult::TimeResult(TimeResult&&) = default;

SharedStorageDatabase::TimeResult::TimeResult(OperationResult result)
    : result(result) {}

SharedStorageDatabase::TimeResult::~TimeResult() = default;

SharedStorageDatabase::TimeResult& SharedStorageDatabase::TimeResult::operator=(
    TimeResult&&) = default;

SharedStorageDatabase::SharedStorageDatabase(
    base::FilePath db_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageDatabaseOptions> options)
    : db_({// Run the database in exclusive mode. Nobody else should be
           // accessing the database while we're running, and this will give
           // somewhat improved perf.
           .exclusive_locking = true,
           // We DCHECK that the page size is valid in the constructor for
           // `SharedStorageOptions`.
           .page_size = options->max_page_size,
           .cache_size = options->max_cache_size}),
      db_path_(std::move(db_path)),
      special_storage_policy_(std::move(special_storage_policy)),
      // We DCHECK that these `options` fields are all positive in the
      // constructor for `SharedStorageOptions`.
      max_entries_per_origin_(int64_t{options->max_entries_per_origin}),
      max_string_length_(static_cast<size_t>(options->max_string_length)),
      max_init_tries_(static_cast<size_t>(options->max_init_tries)),
      max_iterator_batch_size_(
          static_cast<size_t>(options->max_iterator_batch_size)),
      bit_budget_(static_cast<double>(options->bit_budget)),
      budget_interval_(options->budget_interval),
      origin_staleness_threshold_(options->origin_staleness_threshold),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(!is_filebacked() || db_path_.IsAbsolute());
  db_file_status_ = is_filebacked() ? DBFileStatus::kNotChecked
                                    : DBFileStatus::kNoPreexistingFile;
}

SharedStorageDatabase::~SharedStorageDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool SharedStorageDatabase::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open() && !db_.RazeAndClose())
    return false;

  // The file already doesn't exist.
  if (!is_filebacked())
    return true;

  return base::DeleteFile(db_path_);
}

void SharedStorageDatabase::TrimMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.TrimMemory();
}

SharedStorageDatabase::GetResult SharedStorageDatabase::Get(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return `OperationResult::kInitFailure` if the database doesn't
    // exist, but only if it pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return GetResult(OperationResult::kNotFound);
    return GetResult(OperationResult::kInitFailure);
  }

  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the value from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT value FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  statement.BindString(0, origin_str);
  statement.BindString16(1, key);

  if (statement.Step())
    return GetResult(statement.ColumnString16(0), OperationResult::kSuccess);

  if (!statement.Succeeded())
    return GetResult();

  return GetResult(OperationResult::kNotFound);
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Set(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    SetBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  std::string origin_str(SerializeOrigin(context_origin));
  if (HasEntryFor(origin_str, key)) {
    if (behavior == SharedStorageDatabase::SetBehavior::kIgnoreIfPresent) {
      // If we are in a nested transaction, we need to commit, even though we
      // haven't made any changes, so that the failure to set in this case
      // isn't seen as an error (as then the entire stack of transactions
      // will be rolled back and the next transaction within the parent
      // transaction will fail to begin).
      if (db_.transaction_nesting())
        transaction.Commit();
      return OperationResult::kIgnored;
    }

    if (Delete(context_origin, key) != OperationResult::kSuccess)
      return OperationResult::kSqlError;
  } else if (!HasCapacity(origin_str)) {
    return OperationResult::kNoCapacity;
  }

  if (!InsertIntoValuesMapping(origin_str, key, value))
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Append(
    url::Origin context_origin,
    std::u16string key,
    std::u16string tail_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(tail_value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  GetResult get_result = Get(context_origin, key);
  if (get_result.result != OperationResult::kSuccess &&
      get_result.result != OperationResult::kNotFound) {
    return OperationResult::kSqlError;
  }

  std::u16string new_value;
  std::string origin_str(SerializeOrigin(context_origin));

  if (get_result.result == OperationResult::kSuccess) {
    new_value = std::move(get_result.data);
    new_value.append(tail_value);

    if (new_value.size() > max_string_length_)
      return OperationResult::kInvalidAppend;

    if (Delete(context_origin, key) != OperationResult::kSuccess)
      return OperationResult::kSqlError;
  } else {
    new_value = std::move(tail_value);

    if (!HasCapacity(origin_str))
      return OperationResult::kNoCapacity;
  }

  if (!InsertIntoValuesMapping(origin_str, key, new_value))
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Delete(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  if (!HasEntryFor(origin_str, key))
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=? AND key=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, origin_str);
  statement.BindString16(1, key);

  if (!statement.Run())
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/-1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Clear(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  if (!Purge(SerializeOrigin(context_origin)))
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

int64_t SharedStorageDatabase::Length(url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return -1 (to signifiy an error) if the database doesn't exist,
    // but only if it pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0L;
    else
      return -1;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  int64_t length = NumEntries(origin_str);
  if (!length)
    return 0L;

  return length;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Keys(
    const url::Origin& context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      keys_listener(std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      keys_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kSuccess;
    } else {
      keys_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kInitFailure;
    }
  }

  static constexpr char kCountSql[] =
      "SELECT COUNT(*) FROM values_mapping WHERE context_origin=?";

  sql::Statement count_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kCountSql));
  std::string origin_str(SerializeOrigin(context_origin));
  count_statement.BindString(0, origin_str);

  int64_t key_count = 0;
  if (count_statement.Step())
    key_count = count_statement.ColumnInt64(0);

  if (!count_statement.Succeeded()) {
    keys_listener->DidReadEntries(
        /*success=*/false, "SQL database could not retrieve key count.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kSqlError;
  }

  if (key_count > INT_MAX) {
    keys_listener->DidReadEntries(
        /*success=*/false, "Unexpectedly found more than INT_MAX keys.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kTooManyFound;
  }

  if (!key_count) {
    keys_listener->DidReadEntries(
        /*success=*/true,
        /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
        /*total_queued_to_send=*/0);
    return OperationResult::kSuccess;
  }

  static constexpr char kSelectSql[] =
      "SELECT key FROM values_mapping WHERE context_origin=? ORDER BY key";

  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindString(0, origin_str);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
        keys;

    if (saved_first_key_for_next_batch) {
      keys.push_back(
          shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
              saved_first_key_for_next_batch.value(), u""));
      saved_first_key_for_next_batch.reset();
    }

    while (select_statement.Step()) {
      if (keys.size() < max_iterator_batch_size_) {
        keys.push_back(
            shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
                select_statement.ColumnString16(0), u""));
      } else {
        // Cache the current key to use as the start of the next batch, as we're
        // already passing through this step and the next iteration of
        // `statement.Step()`, if there is one, during the next iteration of the
        // outer while loop, will give us the subsequent key.
        saved_first_key_for_next_batch = select_statement.ColumnString16(0);
        has_more_entries = true;
        break;
      }
    }

    if (!select_statement.Succeeded()) {
      keys_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving keys.",
          /*entries=*/{}, /*has_more_entries=*/false,
          static_cast<int>(key_count));
      return OperationResult::kSqlError;
    }

    keys_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                  std::move(keys), has_more_entries,
                                  static_cast<int>(key_count));
  }

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Entries(
    const url::Origin& context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      entries_listener(std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      entries_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kSuccess;
    } else {
      entries_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kInitFailure;
    }
  }

  static constexpr char kCountSql[] =
      "SELECT COUNT(*) FROM values_mapping WHERE context_origin=?";

  sql::Statement count_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kCountSql));
  std::string origin_str(SerializeOrigin(context_origin));
  count_statement.BindString(0, origin_str);

  int64_t entry_count = 0;
  if (count_statement.Step())
    entry_count = count_statement.ColumnInt64(0);

  if (!count_statement.Succeeded()) {
    entries_listener->DidReadEntries(
        /*success=*/false, "SQL database could not retrieve entry count.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kSqlError;
  }

  if (entry_count > INT_MAX) {
    entries_listener->DidReadEntries(
        /*success=*/false, "Unexpectedly found more than INT_MAX entries.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kTooManyFound;
  }

  if (!entry_count) {
    entries_listener->DidReadEntries(
        /*success=*/true,
        /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
        /*total_queued_to_send=*/0);
    return OperationResult::kSuccess;
  }

  static constexpr char kSelectSql[] =
      "SELECT key,value FROM values_mapping WHERE context_origin=? "
      "ORDER BY key";

  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindString(0, origin_str);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;
  absl::optional<std::u16string> saved_first_value_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
        entries;

    if (saved_first_key_for_next_batch) {
      DCHECK(saved_first_value_for_next_batch);
      entries.push_back(
          shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
              saved_first_key_for_next_batch.value(),
              saved_first_value_for_next_batch.value()));
      saved_first_key_for_next_batch.reset();
      saved_first_value_for_next_batch.reset();
    }

    while (select_statement.Step()) {
      if (entries.size() < max_iterator_batch_size_) {
        entries.push_back(
            shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
                select_statement.ColumnString16(0),
                select_statement.ColumnString16(1)));
      } else {
        // Cache the current key and value to use as the start of the next
        // batch, as we're already passing through this step and the next
        // iteration of `statement.Step()`, if there is one, during the next
        // iteration of the outer while loop, will give us the subsequent
        // key-value pair.
        saved_first_key_for_next_batch = select_statement.ColumnString16(0);
        saved_first_value_for_next_batch = select_statement.ColumnString16(1);
        has_more_entries = true;
        break;
      }
    }

    if (!select_statement.Succeeded()) {
      entries_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving entries.",
          /*entries=*/{}, /*has_more_entries=*/false,
          static_cast<int>(entry_count));
      return OperationResult::kSqlError;
    }

    entries_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                     std::move(entries), has_more_entries,
                                     static_cast<int>(entry_count));
  }

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::PurgeMatchingOrigins(
    StorageKeyPolicyMatcherFunction storage_key_matcher,
    base::Time begin,
    base::Time end,
    bool perform_storage_cleanup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(begin, end);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  static constexpr char kSelectSql[] =
      "SELECT context_origin FROM per_origin_mapping "
      "WHERE last_used_time BETWEEN ? AND ? "
      "ORDER BY last_used_time";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindTime(0, begin);
  statement.BindTime(1, end);

  std::vector<std::string> origins;

  while (statement.Step())
    origins.push_back(statement.ColumnString(0));

  if (!statement.Succeeded())
    return OperationResult::kSqlError;

  if (origins.empty())
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  for (const auto& origin : origins) {
    if (storage_key_matcher &&
        !storage_key_matcher.Run(
            blink::StorageKey(url::Origin::Create(GURL(origin))),
            special_storage_policy_.get())) {
      continue;
    }

    if (!Purge(origin))
      return OperationResult::kSqlError;
  }

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  if (perform_storage_cleanup && !Vacuum())
    return OperationResult::kSqlError;

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::PurgeStaleOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(origin_staleness_threshold_, base::TimeDelta());

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  static constexpr char kSelectSql[] =
      "SELECT context_origin FROM per_origin_mapping "
      "WHERE last_used_time<? "
      "ORDER BY last_used_time";
  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindTime(0, clock_->Now() - origin_staleness_threshold_);

  std::vector<std::string> stale_origins;

  while (select_statement.Step())
    stale_origins.push_back(select_statement.ColumnString(0));

  if (!select_statement.Succeeded())
    return OperationResult::kSqlError;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  for (const auto& origin : stale_origins) {
    if (!Purge(origin, /*delete_origin_if_empty=*/true))
      return OperationResult::kSqlError;
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM budget_mapping WHERE time_stamp<?";

  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  delete_statement.BindTime(0, clock_->Now() - budget_interval_);

  if (!delete_statement.Run())
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

std::vector<mojom::StorageUsageInfoPtr> SharedStorageDatabase::FetchOrigins(
    bool exclude_empty_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return {};

  const char* kSelectSql = (exclude_empty_origins)
                               ? "SELECT context_origin,last_used_time,length "
                                 "FROM per_origin_mapping "
                                 "WHERE length>0 ORDER BY context_origin"
                               : "SELECT context_origin,last_used_time,length "
                                 "FROM per_origin_mapping "
                                 "ORDER BY context_origin";

  sql::Statement statement(db_.GetUniqueStatement(kSelectSql));
  std::vector<mojom::StorageUsageInfoPtr> fetched_origin_infos;

  while (statement.Step()) {
    fetched_origin_infos.emplace_back(mojom::StorageUsageInfo::New(
        blink::StorageKey(url::Origin::Create(GURL(statement.ColumnString(0)))),
        statement.ColumnInt64(2) * kSharedStorageEntryTotalBytesMultiplier *
            max_string_length_,
        statement.ColumnTime(1)));
  }

  if (!statement.Succeeded())
    return {};

  return fetched_origin_infos;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::MakeBudgetWithdrawal(url::Origin context_origin,
                                            double bits_debit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(bits_debit, 0.0);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  static constexpr char kInsertSql[] =
      "INSERT INTO budget_mapping(context_origin,time_stamp,bits_debit)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, SerializeOrigin(context_origin));
  statement.BindTime(1, clock_->Now());
  statement.BindDouble(2, bits_debit);

  if (!statement.Run())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

SharedStorageDatabase::BudgetResult SharedStorageDatabase::GetRemainingBudget(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return BudgetResult(bit_budget_, OperationResult::kSuccess);
    else
      return BudgetResult(0.0, OperationResult::kInitFailure);
  }

  static constexpr char kSelectSql[] =
      "SELECT SUM(bits_debit) FROM budget_mapping "
      "WHERE context_origin=? AND time_stamp>=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, SerializeOrigin(context_origin));
  statement.BindTime(1, clock_->Now() - budget_interval_);

  double total_debits = 0.0;
  if (statement.Step())
    total_debits = statement.ColumnDouble(0);

  if (!statement.Succeeded())
    return BudgetResult(0.0, OperationResult::kSqlError);

  return BudgetResult(bit_budget_ - total_debits, OperationResult::kSuccess);
}

SharedStorageDatabase::TimeResult SharedStorageDatabase::GetCreationTime(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return TimeResult(OperationResult::kNotFound);
    else
      return TimeResult(OperationResult::kInitFailure);
  }

  TimeResult result;
  int64_t length = 0L;
  result.result =
      GetOriginInfo(SerializeOrigin(context_origin), &length, &result.time);

  return result;
}

bool SharedStorageDatabase::IsOpenForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.is_open();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::DBStatusForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_status_;
}

bool SharedStorageDatabase::OverrideCreationTimeForTesting(
    url::Origin context_origin,
    base::Time new_creation_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return false;

  std::string origin_str = SerializeOrigin(context_origin);
  int64_t length = 0L;
  base::Time old_creation_time;
  OperationResult result =
      GetOriginInfo(origin_str, &length, &old_creation_time);

  if (result != OperationResult::kSuccess &&
      result != OperationResult::kNotFound) {
    return false;
  }

  // Don't delete or insert for non-existent origin.
  if (result == OperationResult::kNotFound)
    return true;

  return DeleteThenMaybeInsertIntoPerOriginMapping(
      origin_str, new_creation_time, length, /*force_insertion=*/true);
}

void SharedStorageDatabase::OverrideClockForTesting(base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clock);
  clock_ = clock;
}

void SharedStorageDatabase::OverrideSpecialStoragePolicyForTesting(
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  special_storage_policy_ = std::move(special_storage_policy);
}

int64_t SharedStorageDatabase::GetNumBudgetEntriesForTesting(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0;
    else
      return -1;
  }

  static constexpr char kSelectSql[] =
      "SELECT COUNT(*) FROM budget_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, SerializeOrigin(context_origin));

  if (statement.Step())
    return statement.ColumnInt64(0);

  return -1;
}

int64_t SharedStorageDatabase::GetTotalNumBudgetEntriesForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0;
    else
      return -1;
  }

  static constexpr char kSelectSql[] = "SELECT COUNT(*) FROM budget_mapping";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

  if (statement.Step())
    return statement.ColumnInt64(0);

  return -1;
}

bool SharedStorageDatabase::PopulateDatabaseForTesting(url::Origin origin1,
                                                       url::Origin origin2,
                                                       url::Origin origin3) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We use `CHECK_EQ()` and `CHECK()` macros instead of early returns because
  // the latter made the test coverage delta too low.
  CHECK_EQ(OperationResult::kSet,
           Set(origin1, u"key1", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin1, u"key2", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin2, u"key1", u"value2", SetBehavior::kDefault));

  CHECK(OverrideCreationTimeForTesting(  // IN-TEST
      origin2, clock_->Now() - base::Days(1)));

  CHECK_EQ(OperationResult::kSet,
           Set(origin3, u"key1", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin3, u"key2", u"value2", SetBehavior::kDefault));

  CHECK(OverrideCreationTimeForTesting(  // IN-TEST
      origin3, clock_->Now() - base::Days(60)));

  // We return a bool in order to facilitate use of `base::test::TestFuture`
  // with this method.
  return true;
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::LazyInit(
    DBCreationPolicy policy) {
  // Early return in case of previous failure, to prevent an unbounded
  // number of re-attempts.
  if (db_status_ != InitStatus::kUnattempted)
    return db_status_;

  if (policy == DBCreationPolicy::kIgnoreIfAbsent && !DBExists())
    return InitStatus::kUnattempted;

  for (size_t i = 0; i < max_init_tries_; ++i) {
    db_status_ = InitImpl();
    if (db_status_ == InitStatus::kSuccess)
      return db_status_;

    meta_table_.Reset();
    db_.Close();
  }

  return db_status_;
}

bool SharedStorageDatabase::DBExists() {
  DCHECK_EQ(InitStatus::kUnattempted, db_status_);

  if (db_file_status_ == DBFileStatus::kNoPreexistingFile)
    return false;

  // The in-memory case is included in `DBFileStatus::kNoPreexistingFile`.
  DCHECK(is_filebacked());

  // We do not expect `DBExists()` to be called in the case where
  // `db_file_status_ == DBFileStatus::kPreexistingFile`, as then
  // `db_status_ != InitStatus::kUnattempted`, which would force an early return
  // in `LazyInit()`.
  DCHECK_EQ(DBFileStatus::kNotChecked, db_file_status_);

  // The histogram tag must be set before opening.
  db_.set_histogram_tag("SharedStorage");

  if (!db_.Open(db_path_)) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  static const char kSelectSql[] =
      "SELECT COUNT(*) FROM sqlite_schema WHERE type=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindCString(0, "table");

  if (!statement.Step() || statement.ColumnInt(0) == 0) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  db_file_status_ = DBFileStatus::kPreexistingFile;
  return true;
}

bool SharedStorageDatabase::OpenDatabase() {
  // If the database is open, the histogram tag will have already been set in
  // `DBExists()`, since it must be set before opening.
  if (!db_.is_open())
    db_.set_histogram_tag("SharedStorage");

  // base::Unretained is safe here because this SharedStorageDatabase owns
  // the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_.set_error_callback(base::BindRepeating(
      &SharedStorageDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (is_filebacked()) {
    if (!db_.is_open() && !db_.Open(db_path_))
      return false;

    db_.Preload();
  } else {
    if (!db_.OpenInMemory())
      return false;
  }

  return true;
}

void SharedStorageDatabase::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  base::UmaHistogramSparse("Storage.SharedStorage.Database.Error",
                           extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    bool success = Destroy();
    base::UmaHistogramBoolean("Storage.SharedStorage.Database.Destruction",
                              success);
    if (!success) {
      DLOG(FATAL) << "Database destruction failed after catastrophic error:\n"
                  << db_.GetErrorMessage();
    }
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_.GetErrorMessage();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::InitImpl() {
  if (!OpenDatabase())
    return InitStatus::kError;

  // Database should now be open.
  DCHECK(db_.is_open());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LOG(WARNING) << "Shared storage database begin initialization failed.";
    db_.RazeAndClose();
    return InitStatus::kError;
  }

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCurrentVersionNumber) ||
      !InitSchema(db_)) {
    return InitStatus::kError;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Shared storage database is too new.";
    return InitStatus::kTooNew;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (cur_version < kCurrentVersionNumber) {
    LOG(WARNING) << "Shared storage database is too old to be compatible.";
    db_.RazeAndClose();
    return InitStatus::kTooOld;
  }

  // The initialization is complete.
  if (!transaction.Commit()) {
    LOG(WARNING) << "Shared storage database initialization commit failed.";
    db_.RazeAndClose();
    return InitStatus::kError;
  }

  LogInitHistograms();
  return InitStatus::kSuccess;
}

bool SharedStorageDatabase::Vacuum() {
  DCHECK_EQ(InitStatus::kSuccess, db_status_);
  DCHECK_EQ(0, db_.transaction_nesting())
      << "Can not have a transaction when vacuuming.";
  return db_.Execute("VACUUM");
}

bool SharedStorageDatabase::Purge(const std::string& context_origin,
                                  bool delete_origin_if_empty) {
  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  if (!statement.Run())
    return false;

  int64_t length = 0L;
  base::Time creation_time;
  OperationResult result =
      GetOriginInfo(context_origin, &length, &creation_time);

  if (result != OperationResult::kSuccess &&
      result != OperationResult::kNotFound) {
    return false;
  }

  // Don't delete or insert for non-existent origin.
  if (result == OperationResult::kNotFound)
    return true;

  if (!DeleteThenMaybeInsertIntoPerOriginMapping(context_origin, creation_time,
                                                 0L, !delete_origin_if_empty)) {
    return false;
  }

  return transaction.Commit();
}

int64_t SharedStorageDatabase::NumEntries(const std::string& context_origin) {
  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the `length` from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);

  int64_t length = 0;
  if (statement.Step())
    length = statement.ColumnInt64(0);

  return length;
}

bool SharedStorageDatabase::HasEntryFor(const std::string& context_origin,
                                        const std::u16string& key) {
  static constexpr char kSelectSql[] =
      "SELECT 1 FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);
  statement.BindString16(1, key);

  return statement.Step();
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::GetOriginInfo(
    const std::string& context_origin,
    int64_t* out_length,
    base::Time* out_creation_time) {
  DCHECK(out_length);
  DCHECK(out_creation_time);

  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we retrieve only the `length` and `last_used_time`
  // from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length,last_used_time FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);

  if (statement.Step()) {
    *out_length = statement.ColumnInt64(0);
    *out_creation_time = statement.ColumnTime(1);
    return OperationResult::kSuccess;
  }

  if (!statement.Succeeded())
    return OperationResult::kSqlError;
  return OperationResult::kNotFound;
}

bool SharedStorageDatabase::UpdateLength(const std::string& context_origin,
                                         int64_t delta,
                                         bool delete_origin_if_empty) {
  int64_t length = 0L;
  base::Time creation_time;
  OperationResult result =
      GetOriginInfo(context_origin, &length, &creation_time);

  if (result != OperationResult::kSuccess &&
      result != OperationResult::kNotFound) {
    return false;
  }

  if (result == OperationResult::kNotFound) {
    // Don't delete or insert for non-existent origin when we would have
    // decremented the length.
    if (delta < 0L)
      return true;

    // We are creating `context_origin` now.
    creation_time = clock_->Now();
  }

  int64_t new_length = (length + delta > 0L) ? length + delta : 0L;

  return DeleteThenMaybeInsertIntoPerOriginMapping(
      context_origin, creation_time, new_length, !delete_origin_if_empty);
}

bool SharedStorageDatabase::InsertIntoValuesMapping(
    const std::string& context_origin,
    const std::u16string& key,
    const std::u16string& value) {
  static constexpr char kInsertSql[] =
      "INSERT INTO values_mapping(context_origin,key,value)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindString16(1, key);
  statement.BindString16(2, value);

  return statement.Run();
}

bool SharedStorageDatabase::DeleteFromPerOriginMapping(
    const std::string& context_origin) {
  static constexpr char kDeleteSql[] =
      "DELETE FROM per_origin_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  return statement.Run();
}

bool SharedStorageDatabase::InsertIntoPerOriginMapping(
    const std::string& context_origin,
    base::Time creation_time,
    uint64_t length) {
  static constexpr char kInsertSql[] =
      "INSERT INTO per_origin_mapping(context_origin,last_used_time,length)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindTime(1, creation_time);
  statement.BindInt64(2, static_cast<int64_t>(length));

  return statement.Run();
}

bool SharedStorageDatabase::DeleteThenMaybeInsertIntoPerOriginMapping(
    const std::string& context_origin,
    base::Time creation_time,
    uint64_t length,
    bool force_insertion) {
  DCHECK(length >= 0L);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  if (!DeleteFromPerOriginMapping(context_origin))
    return false;

  if ((length || force_insertion) &&
      !InsertIntoPerOriginMapping(context_origin, creation_time, length)) {
    return false;
  }

  return transaction.Commit();
}

bool SharedStorageDatabase::HasCapacity(const std::string& context_origin) {
  return NumEntries(context_origin) < max_entries_per_origin_;
}

void SharedStorageDatabase::LogInitHistograms() {
  base::UmaHistogramBoolean("Storage.SharedStorage.Database.IsFileBacked",
                            is_filebacked());

  if (is_filebacked()) {
    int64_t file_size = 0L;
    if (GetFileSize(db_path_, &file_size)) {
      int64_t file_size_kb = file_size / 1024;
      base::UmaHistogramCounts10M(
          "Storage.SharedStorage.Database.FileBacked.FileSize.KB",
          file_size_kb);

      int64_t file_size_gb = file_size_kb / (1024 * 1024);
      if (file_size_gb) {
        base::UmaHistogramCounts1000(
            "Storage.SharedStorage.Database.FileBacked.FileSize.GB",
            file_size_gb);
      }
    }

    static constexpr char kOriginCountSql[] =
        "SELECT COUNT(*) FROM per_origin_mapping";

    sql::Statement origin_count_statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kOriginCountSql));

    if (origin_count_statement.Step()) {
      int64_t origin_count = origin_count_statement.ColumnInt64(0);
      base::UmaHistogramCounts100000(
          "Storage.SharedStorage.Database.FileBacked.NumOrigins", origin_count);

      static constexpr char kQuartileSql[] =
          "SELECT AVG(length) FROM "
          "(SELECT length FROM per_origin_mapping "
          "ORDER BY length LIMIT ? OFFSET ?)";

      sql::Statement median_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kQuartileSql));
      median_statement.BindInt64(0, 2 - (origin_count % 2));
      median_statement.BindInt64(1, (origin_count - 1) / 2);

      if (median_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Median",
            median_statement.ColumnInt64(0));
      }

      // We use Method 1 from https://en.wikipedia.org/wiki/Quartile to
      // calculate upper and lower quartiles.
      int64_t quartile_limit = 2 - (origin_count % 4) / 2;
      int64_t quartile_offset = (origin_count > 1) ? (origin_count - 2) / 4 : 0;
      sql::Statement q1_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kQuartileSql));
      q1_statement.BindInt64(0, quartile_limit);
      q1_statement.BindInt64(1, quartile_offset);

      if (q1_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q1",
            q1_statement.ColumnInt64(0));
      }

      // We use Method 1 from https://en.wikipedia.org/wiki/Quartile to
      // calculate upper and lower quartiles.
      static constexpr char kUpperQuartileSql[] =
          "SELECT AVG(length) FROM "
          "(SELECT length FROM per_origin_mapping "
          "ORDER BY length DESC LIMIT ? OFFSET ?)";

      sql::Statement q3_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kUpperQuartileSql));
      q3_statement.BindInt64(0, quartile_limit);
      q3_statement.BindInt64(1, quartile_offset);

      if (q3_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q3",
            q3_statement.ColumnInt64(0));
      }

      static constexpr char kMinSql[] =
          "SELECT MIN(length) FROM per_origin_mapping";

      sql::Statement min_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kMinSql));

      if (min_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Min",
            min_statement.ColumnInt64(0));
      }

      static constexpr char kMaxSql[] =
          "SELECT MAX(length) FROM per_origin_mapping";

      sql::Statement max_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kMaxSql));

      if (max_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Max",
            max_statement.ColumnInt64(0));
      }
    }

    static constexpr char kValueCountSql[] =
        "SELECT COUNT(*) FROM values_mapping";

    sql::Statement value_count_statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kValueCountSql));

    if (value_count_statement.Step()) {
      base::UmaHistogramCounts10M(
          "Storage.SharedStorage.Database.FileBacked.NumEntries.Total",
          value_count_statement.ColumnInt64(0));
    }
  }
}

}  // namespace storage
