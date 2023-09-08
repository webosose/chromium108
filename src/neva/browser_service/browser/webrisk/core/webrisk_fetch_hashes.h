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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_FETCH_HASHES_H_
#define NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_FETCH_HASHES_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "neva/browser_service/browser/webrisk/core/webrisk.pb.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace webrisk {

class WebRiskDataStore;

class WebRiskFetchHashes {
 public:
  enum Status {
    kFailed,
    kInvalidKey,
    kSuccess,
  };

  typedef base::RepeatingCallback<void(Status status)> FetchHashStatusCallback;

  WebRiskFetchHashes(const std::string& webrisk_key,
                     const scoped_refptr<WebRiskDataStore>& webrisk_data_store,
                     network::SharedURLLoaderFactory* url_loader_factory,
                     const scoped_refptr<base::SingleThreadTaskRunner>&
                         malware_detection_task_runner,
                     FetchHashStatusCallback callback);
  ~WebRiskFetchHashes();

  void ScheduleComputeDiffRequest(base::TimeDelta update_interval_diff);

 private:
  WebRiskFetchHashes() = delete;

  void ComputeDiffRequest();
  void OnRequestResponse(const std::string& url,
                         std::unique_ptr<std::string> response_body);
  bool ComputeDiffResponse(std::unique_ptr<std::string> response_body,
                           ComputeThreatListDiffResponse& file_format);
  base::TimeDelta UpdateDiffResponse(ComputeThreatListDiffResponse file_format);
  void OnUpdatedDiff(base::TimeDelta result);
  void RunFetchStatusCallback(const WebRiskFetchHashes::Status& status);
  void ScheduleComputeDiffRequestInternal(base::TimeDelta update_interval_diff);
  void ScheduleNextRequest(const base::TimeDelta& interval);
  bool IsUpdateScheduled() const;
  bool ParseJSONToUpdateResponse(const std::string& response_body,
                                 ComputeThreatListDiffResponse& file_format);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::OneShotTimer update_timer_;

  FetchHashStatusCallback fetch_status_callback_;

  std::string webrisk_key_;
  const scoped_refptr<WebRiskDataStore> webrisk_data_store_;
  const scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;
  network::SharedURLLoaderFactory* url_loader_factory_ = nullptr;
  base::WeakPtrFactory<WebRiskFetchHashes> weak_factory_{this};
};

}  // namespace webrisk

#endif  // NEVA_BROWSER_SERVICE_BROWSER_WEBRISK_CORE_WEBRISK_FETCH_HASHES_H_
