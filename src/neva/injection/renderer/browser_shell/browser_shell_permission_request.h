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

#ifndef NEVA_INJECTION_RENDERER_BROWSER_SHELL_BROWSER_SHELL_PERMISSION_REQUEST_H_
#define NEVA_INJECTION_RENDERER_BROWSER_SHELL_BROWSER_SHELL_PERMISSION_REQUEST_H_

#include <string>

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace injections {

class PermissionRequest : public gin::Wrappable<PermissionRequest> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  class Delegate {
   public:
    virtual void AckPermission(bool ack, uint64_t id) = 0;
  };

  PermissionRequest(Delegate* delegate, uint64_t id);
  PermissionRequest(const PermissionRequest&) = delete;
  PermissionRequest& operator=(const PermissionRequest&) = delete;
  ~PermissionRequest() override;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void Allow();
  void Deny();

  Delegate* delegate_;
  const uint64_t id_;
};

}  // namespace injections

#endif  // NEVA_INJECTION_RENDERER_BROWSER_SHELL_BROWSER_SHELL_PERMISSION_REQUEST_H_
