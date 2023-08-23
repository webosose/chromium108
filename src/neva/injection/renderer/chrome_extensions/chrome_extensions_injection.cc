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

#include "neva/injection/renderer/chrome_extensions/chrome_extensions_injection.h"

#include "base/logging.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {
// Access via window.neva
const char kRootObjectName[] = "neva";

// methods
const char kAddEventListenerMethodName[] = "addEventListener";
const char kGetExtensionsInfoMethodName[] = "getExtensionsInfo";
const char kSelectExtensionMethodName[] = "selectExtension";
const char kOnExtensionTabCreatedMethodName[] = "extensionTabCreated";
const char kOnExtensionPopupViewCreatedMethodName[] =
    "extensionPopupViewCreated";

// events
const char kOnExtensionTabCreationRequested[] = "create-extension-tab";
const char kOnExtensionPopupCreationRequested[] = "create-extension-popup";
}  // namespace

namespace injections {

gin::WrapperInfo ChromeExtensionsInjection::kWrapperInfo = {
    gin::kEmbedderNativeGin};

void ChromeExtensionsInjection::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> shell_value =
      global->Get(context, gin::StringToV8(isolate, kRootObjectName))
          .ToLocalChecked();

  if (!shell_value.IsEmpty() && shell_value->IsObject())
    return;

  gin::Handle<ChromeExtensionsInjection> shell = gin::CreateHandle(
      isolate, new ChromeExtensionsInjection(isolate, global));
  global
      ->Set(isolate->GetCurrentContext(),
            gin::StringToV8(isolate, kRootObjectName), shell.ToV8())
      .Check();
}

void ChromeExtensionsInjection::Uninstall(blink::WebLocalFrame* frame) {
  NOTIMPLEMENTED();
}

ChromeExtensionsInjection::ChromeExtensionsInjection(
    v8::Isolate* isolate,
    v8::Local<v8::Object> global) {
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      remote_.BindNewPipeAndPassReceiver());

  remote_->BindClient(receiver_.BindNewPipeAndPassRemote());
}

ChromeExtensionsInjection::~ChromeExtensionsInjection() = default;

void ChromeExtensionsInjection::RunAddEventListener(gin::Arguments* args) {
  VLOG(1) << __func__;

  InjectionEventsEmitter::AddEventListener(args);

  if (HasEventListener(kOnExtensionTabCreationRequested)) {
    for (auto& request_id : pending_tab_creation_requests_)
      DoEmit(kOnExtensionTabCreationRequested, request_id);
    pending_tab_creation_requests_.clear();
  }
}

void ChromeExtensionsInjection::GetExtensionsInfo(gin::Arguments* args) {
  v8::Local<v8::Function> local_func;
  if (!args->GetNext(&local_func)) {
    LOG(ERROR) << __func__ << " called without callback function. Do nothing.";
    return;
  }

  auto callback_ptr = std::make_unique<v8::Persistent<v8::Function>>(
      args->isolate(), local_func);

  remote_->GetExtensionsInfo(
      base::BindOnce(&ChromeExtensionsInjection::OnExtensionsInfo,
                     base::Unretained(this), std::move(callback_ptr)));
}

void ChromeExtensionsInjection::OnExtensionsInfo(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    std::vector<base::Value> infos) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Array> extensions_info_arr =
      v8::Array::New(isolate, infos.size());

  for (size_t i = 0; i < infos.size(); ++i) {
    v8::Local<v8::Object> extension_info_object = v8::Object::New(isolate);
    gin::Dictionary extension_info_dict(isolate, extension_info_object);

    const base::DictionaryValue& dict =
        base::Value::AsDictionaryValue(infos[i]);
    std::string id;
    dict.GetString("id", &id);
    std::string name;
    dict.GetString("name", &name);

    extension_info_dict.Set("id", id);
    extension_info_dict.Set("name", name);
    extensions_info_arr->Set(context, i, extension_info_object).Check();
  }

  v8::Local<v8::Function> local_callback = callback->Get(isolate);
  const int argc = 1;
  v8::Local<v8::Value> argv[] = {extensions_info_arr};
  std::ignore = local_callback->Call(context, wrapper, argc, argv);
}

void ChromeExtensionsInjection::SelectExtension(
    uint64_t tab_id,
    const std::string& extension_id) {
  remote_->OnExtensionSelected(tab_id, extension_id);
}

void ChromeExtensionsInjection::OnExtensionTabCreationRequested(
    uint64_t request_id) {
  VLOG(1) << __func__;

  if (HasEventListener(kOnExtensionTabCreationRequested))
    DoEmit(kOnExtensionTabCreationRequested, request_id);
  else
    pending_tab_creation_requests_.push_back(request_id);
}

void ChromeExtensionsInjection::OnExtensionTabCreated(uint64_t request_id,
                                                      uint64_t tab_id) {
  remote_->OnExtensionTabCreated(request_id, tab_id);
}

void ChromeExtensionsInjection::OnExtensionPopupViewCreated(
    uint64_t popup_view_id) {
  remote_->OnExtensionPopupViewCreated(popup_view_id);
}

void ChromeExtensionsInjection::OnExtensionPopupCreationRequested() {
  VLOG(1) << __func__;
  DoEmit(kOnExtensionPopupCreationRequested);
}

// static
gin::ObjectTemplateBuilder ChromeExtensionsInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ChromeExtensionsInjection>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kAddEventListenerMethodName,
                 &ChromeExtensionsInjection::RunAddEventListener)
      .SetMethod(kSelectExtensionMethodName,
                 &ChromeExtensionsInjection::SelectExtension)
      .SetMethod(kGetExtensionsInfoMethodName,
                 &ChromeExtensionsInjection::GetExtensionsInfo)
      .SetMethod(kOnExtensionTabCreatedMethodName,
                 &ChromeExtensionsInjection::OnExtensionTabCreated)
      .SetMethod(kOnExtensionPopupViewCreatedMethodName,
                 &ChromeExtensionsInjection::OnExtensionPopupViewCreated);
}

}  // namespace injections
