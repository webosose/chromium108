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

#ifndef NEVA_INJECTION_RENDERER_INSTALLABLE_MANAGER_INSTALLABLE_MANAGER_INJECTION_H_
#define NEVA_INJECTION_RENDERER_INSTALLABLE_MANAGER_INSTALLABLE_MANAGER_INJECTION_H_

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "neva/app_runtime/public/mojom/installable_manager.mojom.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace injections {

class InstallableManagerInjection
    : public gin::Wrappable<InstallableManagerInjection> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

  InstallableManagerInjection(blink::WebLocalFrame* web_local_frame);
  InstallableManagerInjection(const InstallableManagerInjection&) = delete;
  InstallableManagerInjection& operator=(const InstallableManagerInjection&) =
      delete;
  ~InstallableManagerInjection() override;

  void GetInfo(gin::Arguments* args);
  void InstallApp(gin::Arguments* args);

 private:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void OnGetInfo(std::unique_ptr<v8::Persistent<v8::Function>> callback,
                 bool installable,
                 bool installed);
  void OnInstallApp(std::unique_ptr<v8::Persistent<v8::Function>> callback,
                    bool success);

  mojo::AssociatedRemote<neva_app_runtime::mojom::InstallableManager>
      installable_manager_;
  base::WeakPtrFactory<InstallableManagerInjection> weak_factory_;
};

}  // namespace injections

#endif  // NEVA_INJECTION_RENDERER_INSTALLABLE_MANAGER_INSTALLABLE_MANAGER_INJECTION_H_
