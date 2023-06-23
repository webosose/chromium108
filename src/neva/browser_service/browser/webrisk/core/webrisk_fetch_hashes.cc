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

#include "neva/browser_service/browser/webrisk/core/webrisk_fetch_hashes.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_info.h"
#include "neva/browser_service/browser/webrisk/core/webrisk.pb.h"
#include "neva/browser_service/browser/webrisk/core/webrisk_data_store.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace webrisk {

namespace {

const char kCompressionTypeRAW[] = "RAW";
const char kApiKeyInvalidResp[] = "API_KEY_INVALID";
constexpr base::TimeDelta kRetryInterval = base::Seconds(30);

#if defined(USE_WEBRISK_DATABASE)
const int kMaxDiffEntries = 0;
const int kMaxDatabaseEntries = 0;
constexpr base::TimeDelta kFetchingTimeout = base::Seconds(3);
#endif

}  // namespace

WebRiskFetchHashes::WebRiskFetchHashes(
    const std::string& webrisk_key,
    scoped_refptr<WebRiskDataStore> webrisk_data_store,
    network::SharedURLLoaderFactory* url_loader_factory,
    FetchHashStatusCallback callback)
    : webrisk_key_(webrisk_key),
      webrisk_data_store_(webrisk_data_store),
      url_loader_factory_(url_loader_factory),
      fetch_status_callback_(std::move(callback)) {}

WebRiskFetchHashes::~WebRiskFetchHashes() = default;

void WebRiskFetchHashes::ComputeDiffRequest() {
  VLOG(2) << __func__;

  const std::string kMethod = "GET";
  const std::string api_endpoint_url = base::StringPrintf(
      "https://webrisk.googleapis.com/v1/threatLists:computeDiff?"
      "threatType=%s"
#if defined(USE_WEBRISK_DATABASE)
      "&constraints.maxDiffEntries=%d"
      "&constraints.maxDatabaseEntries=%d"
#endif
      "&constraints.supportedCompressions=%s"
      "&key=%s",
      WebRiskDataStore::kThreatTypeMalware,
#if defined(USE_WEBRISK_DATABASE)
      kMaxDiffEntries, kMaxDatabaseEntries,
#endif
      kCompressionTypeRAW, webrisk_key_.c_str());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(api_endpoint_url);
  request->method = kMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  url_loader_->SetAllowHttpErrorResults(true);

#if defined(USE_WEBRISK_DATABASE)
  url_loader_->SetTimeoutDuration(kFetchingTimeout);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_,
      base::BindOnce(&WebRiskFetchHashes::OnRequestResponse,
                     base::Unretained(this), api_endpoint_url));
#else
  url_loader_->DownloadToString(
      url_loader_factory_,
      base::BindOnce(&WebRiskFetchHashes::OnRequestResponse,
                     base::Unretained(this), api_endpoint_url),
      WebRiskDataStore::kMaxWebRiskStoreSize);
#endif
}

void WebRiskFetchHashes::OnRequestResponse(
    const std::string& url,
    std::unique_ptr<std::string> response_body) {
  VLOG(2) << __func__ << " URL = " << url;
  ComputeThreatListDiffResponse file_format;
  bool is_computed_diff =
      ComputeDiffResponse(std::move(response_body), file_format);

  if (!is_computed_diff) {
    RunFetchStatusCallback(kFailed);
    ScheduleNextRequest(kRetryInterval);
  } else {
    base::TimeDelta next_update_time = WebRiskDataStore::kDefaultUpdateInterval;
    bool is_updated_diff = UpdateDiffResponse(file_format, next_update_time);
    WebRiskFetchHashes::Status status = is_updated_diff ? kSuccess : kFailed;
    RunFetchStatusCallback(status);
    ScheduleNextRequest(next_update_time);
  }

  url_loader_.reset();
}

bool WebRiskFetchHashes::ComputeDiffResponse(
    std::unique_ptr<std::string> response_body,
    ComputeThreatListDiffResponse& file_format) {
  DCHECK(url_loader_);
  absl::optional<int> response_code;  // Invalid response code.
  int net_error = url_loader_->NetError();
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  VLOG(2) << __func__ << " ContentSize = " << url_loader_->GetContentSize()
          << " Response_code = " << response_code.value()
          << " NetError = " << net_error;
  bool is_data_fetched_successful = response_body && net_error == net::OK &&
                                    response_code &&
                                    response_code == net::HTTP_OK;
  if (!is_data_fetched_successful) {
    if ((response_code == 400) && response_body &&
        (response_body->find(kApiKeyInvalidResp) != std::string::npos)) {
      VLOG(1) << __func__ << " Failed, Invalid API Key !!";
    }
    return false;
  }

  absl::optional<base::Value> response_dict =
      base::JSONReader::Read(*response_body);
  if (!response_dict.has_value()) {
    VLOG(1) << __func__ << ", Failed to response body !!";
    return false;
  }

  if (!ParseJSONToUpdateResponse(*response_body, file_format)) {
    VLOG(1) << __func__ << ", Failed to read response!!";
    return false;
  }
  return true;
}

bool WebRiskFetchHashes::UpdateDiffResponse(
    const ComputeThreatListDiffResponse& file_format,
    base::TimeDelta& next_update_time) {
  if (!webrisk_data_store_->WriteDataToDisk(file_format)) {
    VLOG(1) << __func__ << ", Failed to write to store !!";
    return false;
  }
  next_update_time = webrisk_data_store_->GetNextUpdateTime(
      file_format.recommended_next_diff());
  return true;
}

void WebRiskFetchHashes::ScheduleComputeDiffRequest(base::TimeDelta interval) {
  if (IsUpdateScheduled()) {
    VLOG(1) << __func__ << " Update is already scheduled";
    return;
  }

  ScheduleComputeDiffRequestInternal(interval);
}

void WebRiskFetchHashes::ScheduleComputeDiffRequestInternal(
    base::TimeDelta interval) {
  VLOG(2) << __func__ << ", Interval: " << interval;

  if (interval <= base::TimeDelta()) {
    ComputeDiffRequest();
  } else {
    // Database store is present. Return success.
    RunFetchStatusCallback(kSuccess);
    ScheduleNextRequest(interval);
  }
}

void WebRiskFetchHashes::ScheduleNextRequest(const base::TimeDelta& interval) {
  if (IsUpdateScheduled()) {
    VLOG(1) << __func__ << " Update is already scheduled";
    return;
  }
  update_timer_.Start(FROM_HERE, interval, this,
                      &WebRiskFetchHashes::ComputeDiffRequest);
}

bool WebRiskFetchHashes::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

bool WebRiskFetchHashes::ParseJSONToUpdateResponse(
    const std::string& response_body,
    ComputeThreatListDiffResponse& file_format) {
  absl::optional<base::Value> response_dict =
      base::JSONReader::Read(response_body);

  if (!response_dict.has_value())
    return false;

  const std::string* next_diff =
      response_dict->FindStringKey("recommendedNextDiff");
  if (next_diff)
    file_format.set_recommended_next_diff(*next_diff);

  const std::string* response_type =
      response_dict->FindStringKey("responseType");
  if (response_type && !response_type->compare("RESET"))
    file_format.set_response_type(ComputeThreatListDiffResponse::RESET);

  base::Value* addition_data = response_dict->FindDictKey("additions");
  if (addition_data) {
    // Null check is not required for "ThreatEntryAdditions*" and "Checksum*"
    // as proto implementation will always return valid instances
    ThreatEntryAdditions* additions = file_format.mutable_additions();
    base::Value* raw_hashes = addition_data->FindListKey("rawHashes");
    if (raw_hashes) {
      for (const base::Value& item : raw_hashes->GetList()) {
        auto prefix_size = item.FindIntKey("prefixSize");
        RawHashes* raw_hash_list = additions->add_raw_hashes();
        if (raw_hash_list && prefix_size) {
          raw_hash_list->set_prefix_size(*prefix_size);
          const std::string* hashlist_b64 = item.FindStringKey("rawHashes");
          if (hashlist_b64)
            raw_hash_list->set_raw_hashes(*hashlist_b64);
        }
      }
    }
  }

  const std::string* version_token =
      response_dict->FindStringKey("newVersionToken");
  if (version_token)
    file_format.set_new_version_token(*version_token);

  base::Value* checksum_256 = response_dict->FindDictKey("checksum");
  if (checksum_256) {
    const std::string* sha256 = checksum_256->FindStringKey("sha256");
    if (sha256) {
      ComputeThreatListDiffResponse::Checksum* checksum_sha256 =
          file_format.mutable_checksum();
      checksum_sha256->set_sha256(*sha256);
    }
  }

  return true;
}

void WebRiskFetchHashes::RunFetchStatusCallback(
    const WebRiskFetchHashes::Status& status) {
  if (!fetch_status_callback_.is_null()) {
    fetch_status_callback_.Run(status);
  }
}

}  // namespace webrisk
