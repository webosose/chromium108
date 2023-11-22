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

#ifndef NEVA_INJECTION_RENDERER_BROWSER_CONTROL_CUSTOMUSERAGENT_INJECTION_H_
#define NEVA_INJECTION_RENDERER_BROWSER_CONTROL_CUSTOMUSERAGENT_INJECTION_H_

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "neva/browser_service/public/mojom/customuseragent_service.mojom.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace gin {
class Arguments;
}  // namespace gin

namespace injections {

class CustomUserAgentInjection
    : public gin::Wrappable<CustomUserAgentInjection> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

  explicit CustomUserAgentInjection();
  CustomUserAgentInjection(const CustomUserAgentInjection&) = delete;
  CustomUserAgentInjection& operator=(const CustomUserAgentInjection&) = delete;
  ~CustomUserAgentInjection() override;

  bool GetServerCredentials(gin::Arguments* args);
  bool CreateEncryptedServerCredentials(gin::Arguments* args);

 private:
  static void CreateCustomUserAgentObject(v8::Isolate* isolate,
                                          v8::Local<v8::Object> parent);

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;
  void OnGetServerCredentialsRespond(
      std::unique_ptr<v8::Persistent<v8::Function>> callback,
      const std::string& data_result);
  void OnCreateEncryptedServerCredentialsRespond(
      std::unique_ptr<v8::Persistent<v8::Function>> callback,
      const std::string& encrypt_data);

  mojo::Remote<browser::mojom::CustomUserAgentService> remote_customuseragent_;
};

}  // namespace injections

#endif  // NEVA_INJECTION_RENDERER_BROWSER_CONTROL_CUSTOMUSERAGENT_INJECTION_H_