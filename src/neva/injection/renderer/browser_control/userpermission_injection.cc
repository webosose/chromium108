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

#include "neva/injection/renderer/browser_control/userpermission_injection.h"

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace injections {

namespace {

const char kUserPermissionObjectName[] = "userpermission";

const char kOnPromptResponseMethodName[] = "onpromptresponse";
const char kOnShowPromptCallbackName[] = "onshowprompt";

// Returns true if |maybe| is both a value, and that value is true.
inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();
}

}  // anonymous namespace

gin::WrapperInfo UserPermissionInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void UserPermissionInjection::Install(blink::WebLocalFrame* frame) {
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

  v8::Local<v8::String> userpermission_name =
      gin::StringToV8(isolate, kUserPermissionObjectName);
  if (IsTrue(navigator->Has(context, userpermission_name)))
    return;

  v8::Local<v8::Object> userpermission;
  if (CreateObject(frame, isolate, navigator).ToLocal(&userpermission)) {
    userpermission
        ->Set(context, gin::StringToV8(isolate, kOnShowPromptCallbackName),
              v8::Object::New(isolate))
        .Check();
  }
}

// static
void UserPermissionInjection::Uninstall(blink::WebLocalFrame* frame) {
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
    v8::Local<v8::String> userpermission_name =
        gin::StringToV8(isolate, kUserPermissionObjectName);
    if (IsTrue(navigator->Has(context, userpermission_name)))
      navigator->Delete(context, userpermission_name);
  }
}

UserPermissionInjection::UserPermissionInjection() : listener_receiver_(this) {
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      remote_userpermission_.BindNewPipeAndPassReceiver());

  remote_userpermission_->RegisterListener(base::BindOnce(
      &UserPermissionInjection::OnRegisterListener, base::Unretained(this)));
}

UserPermissionInjection::~UserPermissionInjection() = default;

void UserPermissionInjection::ShowPrompt(const std::string& host,
                                         const std::vector<int32_t>& types) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper))
    return;

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> callback_key(
      gin::StringToV8(isolate, kOnShowPromptCallbackName));

  if (!IsTrue(wrapper->Has(context, callback_key))) {
    LOG(ERROR) << __func__ << "(): no appropriate callback";
    return;
  }

  v8::Local<v8::Value> func_value =
      wrapper->Get(context, callback_key).ToLocalChecked();
  if (!func_value->IsFunction()) {
    LOG(ERROR) << __func__ << "(): callback is not a function";
    return;
  }

  v8::Local<v8::Function> local_callback =
      v8::Local<v8::Function>::Cast(func_value);

  const int argc = 2;
  v8::Local<v8::Value> argv[] = {
      gin::StringToV8(isolate, host),
      gin::Converter<std::vector<int32_t>>::ToV8(isolate, types)};
  std::ignore = local_callback->Call(context, wrapper, argc, argv);
}

// static
v8::MaybeLocal<v8::Object> UserPermissionInjection::CreateObject(
    blink::WebLocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Object> parent) {
  gin::Handle<UserPermissionInjection> userpermission =
      gin::CreateHandle(isolate, new UserPermissionInjection());
  parent
      ->Set(isolate->GetCurrentContext(),
            gin::StringToV8(isolate, kUserPermissionObjectName),
            userpermission.ToV8())
      .Check();
  return userpermission->GetWrapper(isolate);
}

// static
gin::ObjectTemplateBuilder UserPermissionInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<UserPermissionInjection>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kOnPromptResponseMethodName,
                 &UserPermissionInjection::OnPromptResponse);
}

void UserPermissionInjection::OnRegisterListener(
    mojo::PendingAssociatedReceiver<browser::mojom::UserPermissionListener>
        receiver) {
  listener_receiver_.Bind(std::move(receiver));
}

bool UserPermissionInjection::OnPromptResponse(gin::Arguments* args) {
  int32_t response_type = -1;
  if (!args->GetNext(&response_type)) {
    LOG(ERROR) << __func__ << ", wrong argument";
    return false;
  }

  if (response_type <= 0 || response_type > 3) {
    LOG(ERROR) << __func__ << ", Invalid or Error response";
    return false;
  }

  bool result = false;
  remote_userpermission_->OnPromptResponse(response_type, &result);
  return result;
}

}  // namespace injections
