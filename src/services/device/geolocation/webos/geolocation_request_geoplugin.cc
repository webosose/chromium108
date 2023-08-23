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

#include "services/device/geolocation/webos/geolocation_request_geoplugin.h"

#include "base/json/json_reader.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
const char kNetworkProviderUrl[] = "http://www.geoplugin.net/json.gp";
const char kGeopluginLatitude[] = "geoplugin_latitude";
const char kGeopluginLongitude[] = "geoplugin_longitude";
const int kMaxResponseBodySize = 1024 * 1024;
const double kPositionAccuracy = 20000;
}  // namespace

namespace device {

void FormatPositionError(const GURL& server_url,
                         const std::string& message,
                         mojom::Geoposition& position) {
  position.error_code = mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  position.error_message = "Geolocation request failed '";
  position.error_message += server_url.DeprecatedGetOriginAsURL().spec();
  position.error_message += "' : ";
  position.error_message += message;
  position.error_message += ".";
}

GeolocationRequestGeoplugin::GeolocationRequestGeoplugin(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

GeolocationRequestGeoplugin::~GeolocationRequestGeoplugin() = default;

void GeolocationRequestGeoplugin::GeopluginRequest(
    GeopluginRequestCallback callback) {
  if (callback.is_null()) {
    LOG(ERROR) << __func__ << ", Callback for passing response is not given.";
    return;
  }

  callback_ = std::move(callback);
  if (!url_loader_factory_) {
    LOG(ERROR) << __func__ << ", URL loader factory not available.";
    mojom::Geoposition position;
    std::move(callback_).Run(position, false);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kNetworkProviderUrl);
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&GeolocationRequestGeoplugin::OnGeopluginResponse,
                     base::Unretained(this)),
      kMaxResponseBodySize);
}

void GeolocationRequestGeoplugin::OnGeopluginResponse(
    std::unique_ptr<std::string> data) {
  int net_error = url_loader_->NetError();
  mojom::Geoposition position;
  int status_code = 0;
  if (url_loader_->ResponseInfo())
    status_code = url_loader_->ResponseInfo()->headers->response_code();

  if (net_error != net::OK) {
    FormatPositionError(GURL(kNetworkProviderUrl),
                        net::ErrorToShortString(net_error), position);
    std::move(callback_).Run(position, false);
    LOG(ERROR) << __func__ << ", Geolocation response Error Code-" << net_error;
    return;
  }

  if (status_code != 200) {
    std::string message = "Returned error code ";
    message += base::NumberToString(status_code);
    FormatPositionError(GURL(kNetworkProviderUrl), message, position);
    std::move(callback_).Run(position, false);
    LOG(ERROR) << __func__ << ", Invalid Geolocation response status-"
               << status_code;
    return;
  }

  if (!callback_.is_null()) {
    position = ParseServerResponse(std::move(data));
    std::move(callback_).Run(position, false);
  }
}

mojom::Geoposition GeolocationRequestGeoplugin::ParseServerResponse(
    std::unique_ptr<std::string> response_body) {
  mojom::Geoposition position;
  if (!response_body) {
    LOG(ERROR) << __func__ << ", Geoplugin response was empty !";
    return position;
  }

  auto response_result =
      base::JSONReader::ReadAndReturnValueWithError(*response_body);
  if (!response_result.has_value()) {
    LOG(ERROR) << __func__
               << ", JSONReader failed to parse geoplugin response: "
               << response_result.error().message << std::endl;
    return position;
  }

  base::Value response_value = std::move(response_result.value());
  if (!response_value.is_dict()) {
    LOG(ERROR) << __func__
               << ", Unexpected response type: " << response_value.type();
    return position;
  }

  double latitude = 0;
  double longitude = 0;
  const auto latitude_double =
      response_value.FindDoublePath(kGeopluginLatitude);
  if (!latitude_double.has_value()) {
    const std::string* latitude_string =
        response_value.FindStringPath(kGeopluginLatitude);
    if (!latitude_string) {
      LOG(ERROR) << __func__ << ", Invalid geoplugin_latitude!";
      return position;
    }
    latitude = ::atof(latitude_string->c_str());
  } else {
    latitude = latitude_double.value();
  }

  const auto longitude_double =
      response_value.FindDoublePath(kGeopluginLongitude);
  if (!longitude_double.has_value()) {
    const std::string* longitude_string =
        response_value.FindStringPath(kGeopluginLongitude);
    if (!longitude_string) {
      LOG(ERROR) << __func__ << ", Invalid geoplugin_longitude!";
      return position;
    }
    longitude = ::atof(longitude_string->c_str());
  } else {
    longitude = longitude_double.value();
  }

  position.latitude = latitude;
  position.longitude = longitude;
  position.accuracy = kPositionAccuracy;
  position.timestamp = base::Time::Now();
  return position;
}

}  // namespace device
