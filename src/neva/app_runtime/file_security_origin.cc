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

#include "neva/app_runtime/public/file_security_origin.h"

#include "neva/app_runtime/app/app_runtime_main_delegate.h"

namespace neva_app_runtime {

std::string FileSchemeHostForApp(const std::string& app_id) {
  // valid host needs to be lowercase
  std::string app_id_lower_case = base::ToLowerASCII(app_id);

  AppRuntimeContentClient* content_client = GetAppRuntimeContentClient();
  if (content_client)
    return content_client->FileSchemeHostForApp(app_id_lower_case);
  return app_id_lower_case;
}

url::Origin CreateFileSecurityOriginForApp(const std::string& app_id) {
  url::Origin origin = url::Origin::CreateFromNormalizedTuple(
      url::kFileScheme, FileSchemeHostForApp(app_id), 0);
  origin.set_webapp_id(app_id);
  return origin;
}

}  // namespace neva_app_runtime
