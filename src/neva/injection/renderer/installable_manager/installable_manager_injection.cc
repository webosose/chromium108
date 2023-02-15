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

#include "neva/injection/renderer/installable_manager/installable_manager_injection.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace injections {

namespace {

const char kGetInfoMethodName[] = "getInfo";
const char kInstallMethodName[] = "installApp";

// Returns true if |maybe| is both a value, and that value is true.
inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();
}

}  // namespace

gin::WrapperInfo InstallableManagerInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void InstallableManagerInjection::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  VLOG(1) << __func__ << "(): context available";
  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> navigator_name = gin::StringToV8(isolate, "navigator");
  v8::Local<v8::Object> navigator;
  if (!gin::Converter<v8::Local<v8::Object>>::FromV8(
          isolate, global->Get(context, navigator_name).ToLocalChecked(),
          &navigator))
    return;

  if (IsTrue(navigator->Has(context,
                            gin::StringToV8(isolate, "installablemanager"))))
    return;

  v8::Local<v8::Object> manager_local;
  gin::Handle<InstallableManagerInjection> manager =
      gin::CreateHandle(isolate, new InstallableManagerInjection(frame));
  navigator
      ->Set(isolate->GetCurrentContext(),
            gin::StringToV8(isolate, "installablemanager"), manager.ToV8())
      .Check();
  manager->GetWrapper(isolate).ToLocal(&manager_local);
}

// static
void InstallableManagerInjection::Uninstall(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> navigator_name = gin::StringToV8(isolate, "navigator");
  v8::Local<v8::Object> navigator;
  if (gin::Converter<v8::Local<v8::Object>>::FromV8(
          isolate, global->Get(context, navigator_name).ToLocalChecked(),
          &navigator)) {
    v8::Local<v8::String> manager_name =
        gin::StringToV8(isolate, "installablemanager");
    if (IsTrue(navigator->Has(context, manager_name)))
      navigator->Delete(context, manager_name);
  }
}

InstallableManagerInjection::InstallableManagerInjection(
    blink::WebLocalFrame* web_local_frame)
    : weak_factory_(this) {
  content::RenderFrame::FromWebFrame(web_local_frame)
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&installable_manager_);
}

InstallableManagerInjection::~InstallableManagerInjection() = default;

void InstallableManagerInjection::GetInfo(gin::Arguments* args) {
  v8::Local<v8::Function> local_func;
  if (!args->GetNext(&local_func)) {
    LOG(ERROR) << __func__ << "(), wrong arguments list";
    return;
  }

  auto callback_ptr = std::make_unique<v8::Persistent<v8::Function>>(
      args->isolate(), local_func);

  installable_manager_->GetInfo(
      base::BindOnce(&InstallableManagerInjection::OnGetInfo,
                     weak_factory_.GetWeakPtr(), std::move(callback_ptr)));
}

void InstallableManagerInjection::OnGetInfo(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    bool installable,
    bool installed) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper)) {
    LOG(ERROR) << __func__ << "(): can not get wrapper";
    return;
  }

  v8::Local<v8::Context> context = wrapper->GetCreationContextChecked();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> local_callback = callback->Get(isolate);

  const int argc = 2;
  v8::Local<v8::Value> argv[] = {gin::ConvertToV8(isolate, installable),
                                 gin::ConvertToV8(isolate, installed)};
  local_callback->Call(context, wrapper, argc, argv);
}

void InstallableManagerInjection::InstallApp(gin::Arguments* args) {
  v8::Local<v8::Function> local_func;
  if (!args->GetNext(&local_func)) {
    LOG(ERROR) << __func__ << "(), wrong arguments list";
    return;
  }

  auto callback_ptr = std::make_unique<v8::Persistent<v8::Function>>(
      args->isolate(), local_func);

  installable_manager_->InstallApp(
      base::BindOnce(&InstallableManagerInjection::OnInstallApp,
                     weak_factory_.GetWeakPtr(), std::move(callback_ptr)));
}

void InstallableManagerInjection::OnInstallApp(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    bool success) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper)) {
    LOG(ERROR) << __func__ << "(): can not get wrapper";
    return;
  }

  v8::Local<v8::Context> context = wrapper->GetCreationContextChecked();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> local_callback = callback->Get(isolate);

  const int argc = 1;
  v8::Local<v8::Value> argv[] = {gin::ConvertToV8(isolate, success)};
  local_callback->Call(context, wrapper, argc, argv);
}

gin::ObjectTemplateBuilder
InstallableManagerInjection::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<InstallableManagerInjection>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kGetInfoMethodName, &InstallableManagerInjection::GetInfo)
      .SetMethod(kInstallMethodName, &InstallableManagerInjection::InstallApp);
}

}  // namespace injections
