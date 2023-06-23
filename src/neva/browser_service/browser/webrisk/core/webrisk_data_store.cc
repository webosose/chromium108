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

#include "neva/browser_service/browser/webrisk/core/webrisk_data_store.h"

namespace webrisk {

constexpr char WebRiskDataStore::kThreatTypeMalware[];
constexpr size_t WebRiskDataStore::kHashPrefixSize;
constexpr size_t WebRiskDataStore::kMaxWebRiskStoreSize;
constexpr base::TimeDelta WebRiskDataStore::kDefaultUpdateInterval;

base::TimeDelta WebRiskDataStore::GetFirstUpdateTime() {
  if (IsHashPrefixExpired())
    update_time_ = base::TimeDelta();
  return update_time_;
}

base::TimeDelta WebRiskDataStore::GetNextUpdateTime(
    const std::string& recommended_time) {
  base::Time update_time;
  base::Time::FromUTCString(recommended_time.c_str(), &update_time);
  base::TimeDelta next_update_time = update_time - base::Time::Now();
  return std::max(next_update_time, kDefaultUpdateInterval);
}

void WebRiskDataStore::SetNewVersionToken(const std::string& version_token) {
  version_token_ = version_token;
}

}  // namespace webrisk