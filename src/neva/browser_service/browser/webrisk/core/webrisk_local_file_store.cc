// Copyright 2022 LG Electronics, Inc.
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

#include "neva/browser_service/browser/webrisk/core/webrisk_local_file_store.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "neva/browser_service/browser/webrisk/core/webrisk_utils.h"

namespace webrisk {

scoped_refptr<WebRiskDataStore> WebRiskDataStore::Create() {
  return base::MakeRefCounted<WebRiskLocalFileStore>(
      GetFilePath(kWebRiskStoreFileName));
}

WebRiskLocalFileStore::WebRiskLocalFileStore(const base::FilePath& file_path)
    : file_path_(file_path), update_time_(base::TimeDelta()) {}

bool WebRiskLocalFileStore::Initialize() {
  if (base::PathExists(file_path_))
    return ReadFromDisk();
  return true;
}

bool WebRiskLocalFileStore::ReadFromDisk() {
  VLOG(2) << __func__;

  std::string compute_diff_response;
  if (!base::ReadFileToStringWithMaxSize(file_path_, &compute_diff_response,
                                         kMaxWebRiskStoreSize)) {
    return false;
  }

  if (compute_diff_response.empty())
    return false;

  ComputeThreatListDiffResponse file_format;
  if (!file_format.ParseFromString(compute_diff_response))
    return false;

  std::string raw_hashes, hash_list;
  // Need to make the Raw hashes list dynamic
  raw_hashes = file_format.additions().raw_hashes(0).raw_hashes();
  FillHashPrefixListFromRawHashes(raw_hashes);
  update_time_ = GetNextUpdateTime(file_format.recommended_next_diff());
  return true;
}

bool WebRiskLocalFileStore::WriteDataToDisk(
    const ComputeThreatListDiffResponse& file_format) {
  VLOG(2) << __func__;

  ThreatEntryAdditions entry_additions = file_format.additions();
  std::string file_format_string;
  if (!file_format.SerializeToString(&file_format_string)) {
    VLOG(1) << "Unable to serialize the response string !! ";
    return false;
  }

  size_t written = base::WriteFile(file_path_, file_format_string.data(),
                                   file_format_string.size());

  if (file_format_string.size() != written) {
    base::DeleteFile(file_path_);
    VLOG(1) << " Wrote " << written << " byte(s) instead of "
            << file_format_string.size() << " to " << file_path_.value();
    return false;
  }

  std::string raw_hashes, hash_list;
  raw_hashes = file_format.additions().raw_hashes(0).raw_hashes();
  FillHashPrefixListFromRawHashes(raw_hashes);
  return true;
}

void WebRiskLocalFileStore::FillHashPrefixListFromRawHashes(
    const std::string& raw_hashes) {
  std::string hash_list;
  base::Base64Decode(raw_hashes, &hash_list);
  hash_prefix_list_.clear();
  while (!hash_list.empty()) {
    std::string hash_prefix = hash_list.substr(0, kHashPrefixSize);
    hash_prefix_list_.push_back(hash_prefix);
    hash_list.erase(0, kHashPrefixSize);
  }
}

bool WebRiskLocalFileStore::IsHashPrefixListEmpty() {
  if (!hash_prefix_list_.empty()) {
    return false;
  }
  return true;
}

bool WebRiskLocalFileStore::IsHashPrefixExpired() {
  if (!IsHashPrefixListEmpty() && update_time_ > base::TimeDelta())
    return false;
  return true;
}

bool WebRiskLocalFileStore::IsHashPrefixAvailable(
    const std::string& hash_prefix) {
  if (hash_prefix_list_.empty())
    return false;

  if (std::find(hash_prefix_list_.begin(), hash_prefix_list_.end(),
                hash_prefix) != hash_prefix_list_.end()) {
    VLOG(2) << __func__ << " Hash Prefix found!! ";
    return true;
  }

  return false;
}

}  // namespace webrisk