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

#include "neva/app_runtime/browser/permissions/permission_request_impl.h"

#include "base/logging.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"

namespace neva_app_runtime {

PermissionRequestImpl::PermissionRequestImpl(
    permissions::PermissionRequest* request)
    : permission_request_(request),
      requesting_origin_(request->requesting_origin().spec()) {
  switch (permission_request_->request_type()) {
    case permissions::RequestType::kNotifications:
      request_type_ = PermissionRequest::RequestType::kNotifications;
      break;
    case permissions::RequestType::kCameraPanTiltZoom:
    case permissions::RequestType::kCameraStream:
      request_type_ = PermissionRequest::RequestType::kCameraStream;
      break;
    case permissions::RequestType::kMicStream:
      request_type_ = PermissionRequest::RequestType::kMicStream;
      break;
    default:
      request_type_ = PermissionRequest::RequestType::kInvalid;
      LOG(ERROR) << __func__ << " invalid request_type";
  }
}

PermissionRequestImpl::~PermissionRequestImpl() = default;

const std::string& PermissionRequestImpl::RequestingOrigin() const {
  return requesting_origin_;
}

PermissionRequest::RequestType PermissionRequestImpl::GetRequestType() const {
  return request_type_;
}

void PermissionRequestImpl::PermissionGranted(bool is_one_time) {
  permission_request_->PermissionGranted(is_one_time);
}

void PermissionRequestImpl::PermissionDenied() {
  permission_request_->PermissionDenied();
}

void PermissionRequestImpl::Cancelled() {
  permission_request_->Cancelled();
}

void PermissionRequestImpl::RequestFinished() {
  permission_request_->RequestFinished();
}

}  // namespace neva_app_runtime
