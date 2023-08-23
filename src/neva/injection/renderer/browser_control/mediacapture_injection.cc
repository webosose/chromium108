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

#include "neva/injection/renderer/browser_control/mediacapture_injection.h"

#include "base/bind.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace injections {

namespace {

const char kMediaCaptureObjectName[] = "mediacapture";
const char kOnAudioCaptureStateCallbackName[] = "onaudiocapturestate";
const char kOnVideoCaptureStateCallbackName[] = "onvideocapturestate";
const char kOnWindowCaptureStateCallbackName[] = "onwindowcapturestate";
const char kOnDisplayCaptureStateCallbackName[] = "ondisplaycapturestate";

// Returns true if |maybe| is both a value, and that value is true.
inline bool IsTrue(v8::Maybe<bool> maybe) {
  return maybe.IsJust() && maybe.FromJust();
}

}  // anonymous namespace

gin::WrapperInfo MediaCaptureInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void MediaCaptureInjection::Install(blink::WebLocalFrame* frame) {
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

  v8::Local<v8::String> mediacapture_name =
      gin::StringToV8(isolate, kMediaCaptureObjectName);
  if (IsTrue(navigator->Has(context, mediacapture_name)))
    return;

  v8::Local<v8::Object> mediacapture;
  if (CreateObject(frame, isolate, navigator).ToLocal(&mediacapture)) {
    mediacapture
        ->Set(context,
              gin::StringToV8(isolate, kOnAudioCaptureStateCallbackName),
              v8::Object::New(isolate))
        .Check();
    mediacapture
        ->Set(context,
              gin::StringToV8(isolate, kOnVideoCaptureStateCallbackName),
              v8::Object::New(isolate))
        .Check();
    mediacapture
        ->Set(context,
              gin::StringToV8(isolate, kOnWindowCaptureStateCallbackName),
              v8::Object::New(isolate))
        .Check();
    mediacapture
        ->Set(context,
              gin::StringToV8(isolate, kOnDisplayCaptureStateCallbackName),
              v8::Object::New(isolate))
        .Check();
  }
}

// static
void MediaCaptureInjection::Uninstall(blink::WebLocalFrame* frame) {
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
    v8::Local<v8::String> mediacapture_name =
        gin::StringToV8(isolate, kMediaCaptureObjectName);
    if (IsTrue(navigator->Has(context, mediacapture_name)))
      navigator->Delete(context, mediacapture_name);
  }
}

MediaCaptureInjection::MediaCaptureInjection() : listener_receiver_(this) {
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      remote_mediacapture_.BindNewPipeAndPassReceiver());

  remote_mediacapture_->RegisterListener(base::BindOnce(
      &MediaCaptureInjection::OnRegisterListener, base::Unretained(this)));
}

MediaCaptureInjection::~MediaCaptureInjection() = default;

void MediaCaptureInjection::NotifyAudioCaptureState(const bool state) {
  DispatchCaptureState(kOnAudioCaptureStateCallbackName, state);
}

void MediaCaptureInjection::NotifyVideoCaptureState(const bool state) {
  DispatchCaptureState(kOnVideoCaptureStateCallbackName, state);
}

void MediaCaptureInjection::NotifyWindowCaptureState(const bool state) {
  DispatchCaptureState(kOnWindowCaptureStateCallbackName, state);
}

void MediaCaptureInjection::NotifyDisplayCaptureState(const bool state) {
  DispatchCaptureState(kOnDisplayCaptureStateCallbackName, state);
}

// static
v8::MaybeLocal<v8::Object> MediaCaptureInjection::CreateObject(
    blink::WebLocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Object> parent) {
  gin::Handle<MediaCaptureInjection> mediacapture =
      gin::CreateHandle(isolate, new MediaCaptureInjection());
  parent
      ->Set(isolate->GetCurrentContext(),
            gin::StringToV8(isolate, kMediaCaptureObjectName),
            mediacapture.ToV8())
      .Check();
  return mediacapture->GetWrapper(isolate);
}

// static
gin::ObjectTemplateBuilder MediaCaptureInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<MediaCaptureInjection>::GetObjectTemplateBuilder(
      isolate);
}

void MediaCaptureInjection::DispatchCaptureState(
    const std::string& capture_name,
    const bool state) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper))
    return;

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> callback_key(gin::StringToV8(isolate, capture_name));
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

  const int argc = 1;
  v8::Local<v8::Value> argv[] = {gin::ConvertToV8(isolate, state)};
  std::ignore = local_callback->Call(context, wrapper, argc, argv);
}

void MediaCaptureInjection::OnRegisterListener(
    mojo::PendingAssociatedReceiver<browser::mojom::MediaCaptureListener>
        receiver) {
  listener_receiver_.Bind(std::move(receiver));
}

}  // namespace injections
