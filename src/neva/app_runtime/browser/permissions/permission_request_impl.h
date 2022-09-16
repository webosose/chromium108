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

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_

#include <string>

#include "neva/app_runtime/public/permission_prompt.h"

namespace permissions {
class PermissionRequest;
}

namespace neva_app_runtime {

class PermissionRequestImpl : public neva_app_runtime::PermissionRequest {
 public:
  explicit PermissionRequestImpl(permissions::PermissionRequest* request);
  ~PermissionRequestImpl() override;

  // PermissionRequest impelementation
  const std::string& RequestingOrigin() const override;
  PermissionRequest::RequestType GetRequestType() const override;
  void PermissionGranted(bool is_one_time) override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;

 private:
  permissions::PermissionRequest* permission_request_ = nullptr;
  PermissionRequest::RequestType request_type_;
  const std::string requesting_origin_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
