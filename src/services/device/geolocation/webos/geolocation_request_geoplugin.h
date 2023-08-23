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

#ifndef SERVICES_DEVICE_GEOLOCATION_WEBOS_GEOLOCATION_REQUEST_GEOPLUGIN_H_
#define SERVICES_DEVICE_GEOLOCATION_WEBOS_GEOLOCATION_REQUEST_GEOPLUGIN_H_

#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace device {

class GeolocationRequestGeoplugin {
 public:
  typedef base::OnceCallback<void(const mojom::Geoposition&, bool)>
      GeopluginRequestCallback;
  GeolocationRequestGeoplugin(
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~GeolocationRequestGeoplugin();
  void GeopluginRequest(GeopluginRequestCallback callback);
  void OnGeopluginResponse(std::unique_ptr<std::string> data);

 private:
  mojom::Geoposition ParseServerResponse(
      std::unique_ptr<std::string> response_body);
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  GeopluginRequestCallback callback_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace device
#endif  // SERVICES_DEVICE_GEOLOCATION_WEBOS_GEOLOCATION_REQUEST_GEOPLUGIN_H_
