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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_DATA_STORE_H_
#define NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_DATA_STORE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "neva/browser_service/browser/webrisk/core/webrisk.pb.h"

namespace webrisk {

namespace {

const char kWebRiskStoreFileName[] = "webrisk.store";

}  // namespace

class WebRiskDataStore : public base::RefCounted<WebRiskDataStore> {
 public:
  // Threat type string to be sent with the request
  static constexpr char kThreatTypeMalware[] = "MALWARE";

  // Size of hash prefix to be used
  // FIXME: Most hash prefixes are 4 bytes long but
  // some hash prefixes could have any length between 4 and 32 bytes
  static constexpr size_t kHashPrefixSize = 4;

  // The maximum size of webrisk store file size
  static constexpr size_t kMaxWebRiskStoreSize = 1 * 1024 * 1024;  // 1MB

  // The update interval for data store
  static constexpr base::TimeDelta kDefaultUpdateInterval = base::Hours(1);

  typedef base::OnceCallback<void(bool status)> CheckUrlCallback;

  static scoped_refptr<WebRiskDataStore> Create();

  virtual bool Initialize() = 0;
  virtual bool WriteDataToDisk(
      const ComputeThreatListDiffResponse& file_format) = 0;
  virtual bool IsHashPrefixAvailable(const std::string& hash_prefix) = 0;
  virtual bool IsHashPrefixExpired() = 0;

#if defined(USE_WEBRISK_DATABASE)
  virtual bool InsertOrUpdateVersionToken(const std::string& version_token,
                                          bool is_insert_token) = 0;
  virtual std::string GetVersionToken() = 0;
  virtual bool MigrateDataFromLocalFile() = 0;
#endif

  base::TimeDelta GetFirstUpdateTime();
  base::TimeDelta GetNextUpdateTime(const std::string& recommended_time);

  base::TimeDelta update_time_;

 protected:
  friend class RefCounted<WebRiskDataStore>;
  WebRiskDataStore() = default;
  virtual ~WebRiskDataStore() = default;
};

}  // namespace webrisk

#endif  // NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_DATA_STORE_H_