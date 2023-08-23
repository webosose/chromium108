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

#ifndef NEVA_INJECTION_RENDERER_BROWSER_CONTROL_USERPERMISSION_INJECTION_H_
#define NEVA_INJECTION_RENDERER_BROWSER_CONTROL_USERPERMISSION_INJECTION_H_

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "neva/browser_service/public/mojom/userpermission_service.mojom.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace gin {
class Arguments;
}  // namespace gin

namespace injections {

class UserPermissionInjection : public gin::Wrappable<UserPermissionInjection>,
                                public browser::mojom::UserPermissionListener {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

  explicit UserPermissionInjection();
  UserPermissionInjection(const UserPermissionInjection&) = delete;
  UserPermissionInjection& operator=(const UserPermissionInjection&) = delete;
  ~UserPermissionInjection() override;

  // browser::mojom::UserPermissionListener
  void ShowPrompt(const std::string& host,
                  const std::vector<int32_t>& types) override;

 private:
  static v8::MaybeLocal<v8::Object> CreateObject(blink::WebLocalFrame* frame,
                                                 v8::Isolate* isolate,
                                                 v8::Local<v8::Object> parent);

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  bool OnPromptResponse(gin::Arguments* args);
  void OnRegisterListener(
      mojo::PendingAssociatedReceiver<browser::mojom::UserPermissionListener>
          receiver);

  blink::WebLocalFrame* web_local_frame_ = nullptr;

  mojo::AssociatedReceiver<browser::mojom::UserPermissionListener>
      listener_receiver_;

  mojo::Remote<browser::mojom::UserPermissionService> remote_userpermission_;
};

}  // namespace injections

#endif  // NEVA_INJECTION_RENDERER_BROWSER_CONTROL_USERPERMISSION_INJECTION_H_
