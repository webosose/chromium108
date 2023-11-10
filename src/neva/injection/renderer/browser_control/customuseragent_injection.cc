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

#include "neva/injection/renderer/browser_control/customuseragent_injection.h"

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace injections {

namespace {

const char kCustomUserAgentObjectName[] = "customuseragent";

const char kGetServerCredentialsMethodName[] = "getServerCredentials";
const char kCreateEncryptedServerCredentialsMethodName[] =
    "createEncryptedServerCredentials";
// Returns true if |maybe| is both a value, and that value is true.
inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();
}

}  // anonymous namespace

gin::WrapperInfo CustomUserAgentInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void CustomUserAgentInjection::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> navigator_name = gin::StringToV8(isolate, "navigator");
  v8::Local<v8::Object> navigator;
  if (!gin::Converter<v8::Local<v8::Object>>::FromV8(
          isolate, global->Get(context, navigator_name).ToLocalChecked(),
          &navigator))
    return;

  v8::Local<v8::String> customuseragent_name =
      gin::StringToV8(isolate, kCustomUserAgentObjectName);
  if (IsTrue(navigator->Has(context, customuseragent_name)))
    return;

  CreateCustomUserAgentObject(isolate, navigator);
}

// static
void CustomUserAgentInjection::Uninstall(blink::WebLocalFrame* frame) {
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
    v8::Local<v8::String> customuseragent_name =
        gin::StringToV8(isolate, kCustomUserAgentObjectName);
    if (IsTrue(navigator->Has(context, customuseragent_name)))
      navigator->Delete(context, customuseragent_name);
  }
}

// static
void CustomUserAgentInjection::CreateCustomUserAgentObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> parent) {
  gin::Handle<CustomUserAgentInjection> customuseragent =
      gin::CreateHandle(isolate, new CustomUserAgentInjection());
  parent
      ->Set(isolate->GetCurrentContext(),
            gin::StringToV8(isolate, kCustomUserAgentObjectName),
            customuseragent.ToV8())
      .Check();
}

CustomUserAgentInjection::CustomUserAgentInjection() {
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      remote_customuseragent_.BindNewPipeAndPassReceiver());
}

CustomUserAgentInjection::~CustomUserAgentInjection() = default;

bool CustomUserAgentInjection::GetServerCredentials(gin::Arguments* args) {
  v8::Local<v8::Function> local_func;
  if (!args->GetNext(&local_func)) {
    LOG(ERROR) << __func__ << ", wrong argument";
    return false;
  }

  auto callback_ptr = std::make_unique<v8::Persistent<v8::Function>>(
      args->isolate(), local_func);
  remote_customuseragent_->GetServerCredentials(
      base::BindOnce(&CustomUserAgentInjection::OnGetServerCredentialsRespond,
                     base::Unretained(this), std::move(callback_ptr)));
  return true;
}

bool CustomUserAgentInjection::CreateEncryptedServerCredentials(
    gin::Arguments* args) {
  std::string server_input_data, cipher_key;
  if (!args->GetNext(&server_input_data) || !args->GetNext(&cipher_key)) {
    LOG(ERROR) << __func__ << ", wrong argument";
    return false;
  }

  v8::Local<v8::Function> local_func;
  if (!args->GetNext(&local_func)) {
    LOG(ERROR) << __func__ << ", wrong argument";
    return false;
  }

  auto callback_ptr = std::make_unique<v8::Persistent<v8::Function>>(
      args->isolate(), local_func);
  remote_customuseragent_->CreateEncryptedServerCredentials(
      server_input_data, cipher_key,
      base::BindOnce(
          &CustomUserAgentInjection::OnCreateEncryptedServerCredentialsRespond,
          base::Unretained(this), std::move(callback_ptr)));
  return true;
}

gin::ObjectTemplateBuilder CustomUserAgentInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<CustomUserAgentInjection>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kCreateEncryptedServerCredentialsMethodName,
                 &CustomUserAgentInjection::CreateEncryptedServerCredentials)
      .SetMethod(kGetServerCredentialsMethodName,
                 &CustomUserAgentInjection::GetServerCredentials);
}

void CustomUserAgentInjection::OnGetServerCredentialsRespond(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    const std::string& data_result) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper)) {
    LOG(ERROR) << __func__ << "(): can not get wrapper";
    return;
  }

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> local_callback = callback->Get(isolate);

  const int argc = 1;
  v8::Local<v8::Value> result;
  if (gin::TryConvertToV8(isolate, data_result, &result))
    local_callback->Call(context, wrapper, argc, &result);
}

void CustomUserAgentInjection::OnCreateEncryptedServerCredentialsRespond(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    const std::string& encrypt_data) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper)) {
    LOG(ERROR) << __func__ << "(): can not get wrapper";
    return;
  }

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> local_callback = callback->Get(isolate);

  const int argc = 1;
  v8::Local<v8::Value> result;
  if (gin::TryConvertToV8(isolate, encrypt_data, &result))
    local_callback->Call(context, wrapper, argc, &result);
}

}  // namespace injections
