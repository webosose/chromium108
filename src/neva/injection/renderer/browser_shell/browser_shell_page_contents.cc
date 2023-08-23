// Copyright 2021 LG Electronics, Inc.
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

#include "neva/injection/renderer/browser_shell/browser_shell_page_contents.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "neva/browser_shell/service/public/mojom/browser_shell_constants.mojom.h"
#include "neva/injection/renderer/browser_shell/browser_shell_login.h"
#include "neva/injection/renderer/browser_shell/browser_shell_permission_request.h"
#include "url/gurl.h"

namespace injections {

namespace events {
const char kEnterHtmlFullscreen[] = "enter-html-fullscreen";
const char kDialog[] = "dialog";
const char kDidFailLoad[] = "did-fail-load";
const char kDidFinishLoad[] = "did-finish-load";
const char kDidStartLoading[] = "did-start-loading";
const char kDidStartNavigation[] = "did-start-navigation";
const char kDidStopLoading[] = "did-stop-loading";
const char kDidUpdateFaviconUrl[] = "did-update-favicon-url";
const char kDOMReady[] = "dom-ready";
const char kLeaveHtmlFullscreen[] = "leave-html-fullscreen";
const char kLoadProgressChanged[] = "load-progress-changed";
const char kOnNavigationEntryCommitted[] = "navigation-entry-committed";
const char kOnAcceptedLanguagesChangedEvent[] = "accepted-languages-changed";
const char kAuthChallengeEvent[] = "login";
const char kOnFocusChanged[] = "focus-changed";
const char kOnNewWindowOpen[] = "newwindow";
const char kOnRendererUnresponsive[] = "unresponsive";
const char kOnRendererResponsive[] = "responsive";
const char kOnPermissionRequest[] = "permissionrequest";
const char kOnZoomFactorChanged[] = "zoomchange";
const char kPageTitleUpdated[] = "page-title-updated";
const char kOnCloseEvent[] = "close";
const char kOnExitEvent[] = "exit";
}  // namespace events

gin::WrapperInfo BrowserShellPageContents::kWrapperInfo = {
    gin::kEmbedderNativeGin};

char BrowserShellPageContents::kIdPropertyName[] = "id";
char BrowserShellPageContents::kCanGoBackPropertyName[] = "canGoBack";
char BrowserShellPageContents::kCanGoForwardPropertyName[] = "canGoForward";
char BrowserShellPageContents::kErrorPageHidingPropertyName[] =
    "errorPageHiding";
char BrowserShellPageContents::kIsActivePropertyName[] = "isActive";
char BrowserShellPageContents::kIsAlivePropertyName[] = "isAlive";
char BrowserShellPageContents::kUrlPropertyName[] = "url";
char BrowserShellPageContents::kUserAgentPropertyName[] = "userAgent";
char BrowserShellPageContents::kZoomFactorProperyName[] = "zoomFactor";

char BrowserShellPageContents::kActivateMethodName[] = "activate";
char BrowserShellPageContents::kCaptureVisibleRegionMethodName[] =
    "captureVisibleRegion";
char BrowserShellPageContents::kClearDataMethodName[] = "clearData";
char BrowserShellPageContents::kCloseNowMethodName[] = "closeNow";
char BrowserShellPageContents::kDeactivateMethodName[] = "deactivate";
char BrowserShellPageContents::kExecuteJavaScriptInAllFramesMethodName[] =
    "executeJavaScriptInAllFrames";
char BrowserShellPageContents::kExecuteJavaScriptInMainFrameMethodName[] =
    "executeJavaScriptInMainFrame";
char BrowserShellPageContents::kGoBackMethodName[] = "goBack";
char BrowserShellPageContents::kGoForwardMethodName[] = "goForward";
char BrowserShellPageContents::kLoadFileMethodName[] = "loadFile";
char BrowserShellPageContents::kLoadURLMethodName[] = "loadURL";
char BrowserShellPageContents::kReloadMethodName[] = "reload";
char BrowserShellPageContents::kResumeDOMMethodName[] = "resumeDOM";
char BrowserShellPageContents::kResumeMediaMethodName[] = "resumeMedia";
char BrowserShellPageContents::kSetAcceptedLanguagesMethodName[] =
    "setAcceptedLanguages";
char BrowserShellPageContents::kSetFocusMethodName[] = "setFocus";
char BrowserShellPageContents::kSetPageBaseBackgroundColorMethodName[] =
    "setPageBaseBackgroundColor";
char BrowserShellPageContents::kSetSuppressSSLErrorPolicyMethodName[] =
    "setSuppressSSLErrorPolicy";
char BrowserShellPageContents::kStopMethodName[] = "stop";
char BrowserShellPageContents::kSuspendDOMMethodName[] = "suspendDOM";
char BrowserShellPageContents::kSuspendMediaMethodName[] = "suspendMedia";

// static
void BrowserShellPageContents::ConstructorCallback(
    mojo::Remote<browser_shell::mojom::ShellService>* shell_service,
    gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::HandleScope handle_scope(isolate);

  if (!args->IsConstructCall()) {
    isolate->ThrowException(v8::Exception::Error(
        gin::StringToV8(isolate, "Must be a constructor call")));
    return;
  }

  CreateParams params;
  std::string json;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> json_value = args->PeekNext();

  if (!json_value.IsEmpty() && json_value->IsObject()) {
    v8::MaybeLocal<v8::String> maybe_json_str =
        v8::JSON::Stringify(context, json_value);
    v8::Local<v8::String> json_str;
    if (maybe_json_str.ToLocal(&json_str))
      json = gin::V8ToString(args->isolate(), json_str);

    v8::Local<v8::Object> json_obj = v8::Local<v8::Object>::Cast(json_value);
    gin::Dictionary json_dict(isolate, json_obj);
    std::ignore = json_dict.Get("error-page-hiding", &params.error_page_hiding);
  }

  mojo::Remote<browser_shell::mojom::PageContents> remote_contents;
  auto pending_receiver = remote_contents.BindNewPipeAndPassReceiver();
  auto* shell_page_contents =
      new BrowserShellPageContents(isolate, std::move(remote_contents), params);

  (*shell_service)
      ->CreatePageContents(
          std::move(pending_receiver), json,
          base::BindOnce(&BrowserShellPageContents::Setup,
                         base::Unretained(shell_page_contents)));

  gin::Handle<injections::BrowserShellPageContents> handle =
      gin::CreateHandle(isolate, shell_page_contents);

  if (!handle.IsEmpty())
    args->Return(handle.ToV8());
}

BrowserShellPageContents::BrowserShellPageContents(
    v8::Isolate* isolate,
    mojo::Remote<browser_shell::mojom::PageContents> remote,
    const CreateParams& params,
    uint64_t id)
    : id_(id),
      error_page_hiding_(params.error_page_hiding),
      is_main_contents_(params.is_main_contents),
      remote_(std::move(remote)),
      client_receiver_(this) {
  remote_->BindClient(base::BindOnce(&BrowserShellPageContents::SetupClient,
                                     base::Unretained(this)));
}

BrowserShellPageContents::~BrowserShellPageContents() = default;

void BrowserShellPageContents::Activate() {
  if (remote_.is_bound()) {
    remote_->Activate();
    remote_->SyncActiveState(&is_active_);
  }
}

void BrowserShellPageContents::CaptureVisibleRegion(gin::Arguments* args) {
  if (!remote_.is_bound())
    return;

  const char default_format[] = "jpeg";
  const int32_t default_quality = 90;

  v8::Local<v8::Function> local_func;
  v8::Local<v8::Object> details;
  std::string format(default_format);
  int32_t quality = default_quality;

  v8::Isolate* isolate = args->isolate();
  if (!args->GetData(&local_func)) {
    if (!args->GetNext(&details))
      return;

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Value> format_value;
    if (details->Get(context, gin::StringToV8(isolate, "format"))
            .ToLocal(&format_value)) {
      if (!gin::Converter<std::string>::FromV8(isolate, format_value, &format))
        format = std::string(default_format);
    }

    v8::Local<v8::Value> quality_value;
    if (details->Get(context, gin::StringToV8(isolate, "quality"))
            .ToLocal(&quality_value)) {
      if (!gin::Converter<int32_t>::FromV8(isolate, quality_value, &quality))
        quality = default_quality;
    }

    if (!args->GetNext(&local_func))
      return;
  }

  auto callback_ptr =
      std::make_unique<v8::Persistent<v8::Function>>(isolate, local_func);

  remote_->CaptureVisibleRegion(
      format, quality,
      base::BindOnce(&BrowserShellPageContents::OnCaptureVisibleRegionReply,
                     base::Unretained(this), std::move(callback_ptr)));
}

void BrowserShellPageContents::ClearData(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::string clear_options;
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> clear_options_value;
    args->GetNext(&clear_options_value);
    if (!clear_options_value.IsEmpty()) {
      v8::MaybeLocal<v8::String> maybe_clear_options_str =
          v8::JSON::Stringify(context, clear_options_value);
      v8::Local<v8::String> clear_options_str;
      if (maybe_clear_options_str.ToLocal(&clear_options_str))
        clear_options = gin::V8ToString(args->isolate(), clear_options_str);
    }
  }

  std::string clear_data_type_set;
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> clear_data_type_set_value;
    args->GetNext(&clear_data_type_set_value);
    if (!clear_data_type_set_value.IsEmpty()) {
      v8::MaybeLocal<v8::String> maybe_clear_data_type_set_str =
          v8::JSON::Stringify(context, clear_data_type_set_value);
      v8::Local<v8::String> clear_data_type_set_str;
      if (maybe_clear_data_type_set_str.ToLocal(&clear_data_type_set_str))
        clear_data_type_set =
            gin::V8ToString(args->isolate(), clear_data_type_set_str);
    }
  }

  remote_->ClearData(clear_options, clear_data_type_set);
}

void BrowserShellPageContents::Deactivate() {
  if (!remote_.is_bound() || is_main_contents_)
    return;

  remote_->Deactivate();
  remote_->SyncActiveState(&is_active_);
}

bool BrowserShellPageContents::GetActiveState() {
  if (!remote_.is_bound())
    return false;

  remote_->SyncActiveState(&is_active_);
  return is_active_;
}

bool BrowserShellPageContents::GetErrorPageHiding() const {
  return error_page_hiding_;
}

uint64_t BrowserShellPageContents::GetID() {
  if (!id_ && remote_.is_bound() && remote_.is_connected())
    remote_->SyncId(&id_);
  return id_;
}

void BrowserShellPageContents::Setup(
    uint64_t id,
    browser_shell::mojom::PageContentsCreationInfoPtr info) {
  id_ = id;
  is_active_ = info->active_state;
  error_page_hiding_ = info->error_page_hiding;
  user_agent_ = std::move(info->user_agent);
  zoom_factor_ = info->zoom_factor;
}

void BrowserShellPageContents::SetupClient(
    mojo::PendingAssociatedReceiver<browser_shell::mojom::PageContentsClient>
        receiver) {
  client_receiver_.Bind(std::move(receiver));
}

void BrowserShellPageContents::OnLoadURL(const std::string& url) {
  NOTIMPLEMENTED();
}

void BrowserShellPageContents::EnterHtmlFullscreen() {
  DoEmit(events::kEnterHtmlFullscreen);
}

void BrowserShellPageContents::DidFailLoad(const std::string& url,
                                           const std::string& error,
                                           int32_t error_code) {
  url_string_ = url;
  DoEmit(events::kDidFailLoad, url, error, error_code);
}

void BrowserShellPageContents::DidFinishLoad(const std::string& url) {
  url_string_ = url;
  DoEmit(events::kDidFinishLoad, url);
}

void BrowserShellPageContents::DidStartLoading() {
  DoEmit(events::kDidStartLoading);
}

void BrowserShellPageContents::OnClose() {
  DoEmit(events::kOnCloseEvent);
}

void BrowserShellPageContents::OnExit(const std::string& reason) {
  DoEmit(events::kOnExitEvent, reason);
}

void BrowserShellPageContents::OnNavigationEntryCommitted(
    browser_shell::mojom::PageContentsStatePtr state) {
  state_.can_go_back = state->can_go_back;
  state_.can_go_forward = state->can_go_forward;
  DoEmit(events::kOnNavigationEntryCommitted);
}

void BrowserShellPageContents::OnAcceptedLanguagesChanged(
    const std::string& languages) {
  DoEmit(events::kOnAcceptedLanguagesChangedEvent, languages);
}

void BrowserShellPageContents::AckAuthChallenge(const std::string& login,
                                                const std::string& passwd,
                                                const std::string& url) {
  if (remote_.is_bound())
    remote_->AckAuthChallenge(login, passwd, url);
}

void BrowserShellPageContents::OnAuthChallenge(
    browser_shell::mojom::AuthChallengePtr challenge) {
  if (!GetListenerCount(events::kAuthChallengeEvent)) {
    AckAuthChallenge("", "", "");
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);

  gin::Handle<BrowserShellLogin> login_obj =
      gin::CreateHandle(isolate, new BrowserShellLogin(this, std::move(challenge)));
  DoEmit(events::kAuthChallengeEvent, login_obj.ToV8());
}

void BrowserShellPageContents::OnFocusChanged(bool is_focused) {
  DoEmit(events::kOnFocusChanged, is_focused);
}

void BrowserShellPageContents::OnNewWindowOpen(
    uint64_t id,
    browser_shell::mojom::WindowInfoPtr window_info) {
  if (!remote_.is_bound())
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> window_info_v8 = v8::Object::New(isolate);

  window_info_v8
      ->Set(context, gin::StringToV8(isolate, "targetUrl"),
            gin::StringToV8(isolate, window_info->target_url))
      .Check();
  window_info_v8
      ->Set(context, gin::StringToV8(isolate, "initialWidth"),
            v8::Integer::New(isolate, window_info->initial_width))
      .Check();
  window_info_v8
      ->Set(context, gin::StringToV8(isolate, "initialHeight"),
            v8::Integer::New(isolate, window_info->initial_height))
      .Check();
  window_info_v8
      ->Set(context, gin::StringToV8(isolate, "name"),
            gin::StringToV8(isolate, window_info->name))
      .Check();
  window_info_v8
      ->Set(context, gin::StringToV8(isolate, "windowOpenDisposition"),
            gin::StringToV8(isolate, window_info->window_open_disposition))
      .Check();

  mojo::Remote<browser_shell::mojom::PageContents> remote_new_contents;
  auto pending_receiver_new_contents =
      remote_new_contents.BindNewPipeAndPassReceiver();

  BrowserShellPageContents::CreateParams page_contents_params;
  page_contents_params.error_page_hiding = error_page_hiding_;
  auto* new_page_contents = new BrowserShellPageContents(
      isolate, std::move(remote_new_contents), page_contents_params);

  remote_->BindNewPageContentsById(
      std::move(pending_receiver_new_contents), id,
      base::BindOnce(&BrowserShellPageContents::Setup,
                     base::Unretained(new_page_contents)));

  gin::Handle<injections::BrowserShellPageContents> handle =
      gin::CreateHandle(isolate, new_page_contents);

  DoEmit(events::kOnNewWindowOpen, handle.ToV8(), window_info_v8);
}

void BrowserShellPageContents::OnRendererUnresponsive() {
  DoEmit(events::kOnRendererUnresponsive);
}

void BrowserShellPageContents::OnRendererResponsive() {
  DoEmit(events::kOnRendererResponsive);
}

void BrowserShellPageContents::OnZoomFactorChanged(double zoom_factor) {
  zoom_factor_ = zoom_factor;
  DoEmit(events::kOnZoomFactorChanged, zoom_factor_);
}

void BrowserShellPageContents::AckPermission(bool ack, uint64_t id) {
  if (remote_.is_bound())
    remote_->AckPermission(ack, id);
}

void BrowserShellPageContents::OnPermissionRequest(
    const std::string& permission,
    uint64_t id) {
  if (!GetListenerCount(events::kOnPermissionRequest)) {
    AckPermission(false, id);
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> request_permission = v8::Object::New(isolate);

  request_permission
      ->Set(context, gin::StringToV8(isolate, "permission"),
            gin::StringToV8(isolate, permission))
      .Check();

  gin::Handle<PermissionRequest> request_obj =
      gin::CreateHandle(isolate, new PermissionRequest(this, id));
  request_permission
      ->Set(context, gin::StringToV8(isolate, "request"), request_obj.ToV8())
      .Check();

  DoEmit(events::kOnPermissionRequest, request_permission);
}

void BrowserShellPageContents::DidStartNavigation(const std::string& url) {
  url_string_ = url;
  DoEmit(events::kDidStartNavigation, url);
}

void BrowserShellPageContents::DidStopLoading() {
  DoEmit(events::kDidStopLoading);
}

void BrowserShellPageContents::DidUpdateFaviconUrl(
    std::vector<browser_shell::mojom::FaviconInfoPtr> favicons) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Array> favicon_info_arr =
      v8::Array::New(isolate, favicons.size());

  for (size_t i = 0; i < favicons.size(); ++i) {
    v8::Local<v8::Object> favicon_info_object = v8::Object::New(isolate);
    gin::Dictionary favicon_info_dict(isolate, favicon_info_object);
    favicon_info_dict.Set("url", favicons[i]->url);
    favicon_info_dict.Set("type", favicons[i]->type);

    const size_t favicon_sizes_len = favicons[i]->sizes.size();
    const auto& favicon_sizes = favicons[i]->sizes;
    v8::Local<v8::Array> favicon_sizes_arr =
        v8::Array::New(isolate, favicon_sizes_len);

    for (size_t k = 0; k < favicon_sizes_len; ++k) {
      v8::Local<v8::Object> favicon_size_object = v8::Object::New(isolate);
      gin::Dictionary favicon_size_dict(isolate, favicon_size_object);
      favicon_size_dict.Set("width", favicon_sizes[k]->width);
      favicon_size_dict.Set("height", favicon_sizes[k]->height);
      favicon_sizes_arr->Set(context, k, favicon_size_object).Check();
    }

    favicon_info_dict.Set("sizes", favicon_sizes_arr.As<v8::Object>());
    favicon_info_arr->Set(context, i, favicon_info_object).Check();
  }

  DoEmit(events::kDidUpdateFaviconUrl, favicon_info_arr.As<v8::Object>());
}

void BrowserShellPageContents::DOMReady() {
  DoEmit(events::kDOMReady);
}

void BrowserShellPageContents::RunJSDialog(const std::string& type,
                                           const std::string& message) {
  if (RunGetListenerCount(events::kDialog) == 0) {
    CloseJSDialog(false, std::string());
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context))
    return;
  v8::Context::Scope context_scope(context);
  gin::Handle<DialogController> dialog_controller =
      gin::CreateHandle(isolate, new DialogController(this));

  DoEmit(events::kDialog, type, message, dialog_controller.ToV8());
}

void BrowserShellPageContents::LeaveHtmlFullscreen() {
  DoEmit(events::kLeaveHtmlFullscreen);
}

void BrowserShellPageContents::LoadProgressChanged(uint32_t progress) {
  DoEmit(events::kLoadProgressChanged, progress);
}

void BrowserShellPageContents::TitleUpdated(const std::string& title) {
  DoEmit(events::kPageTitleUpdated, title);
}

void BrowserShellPageContents::CloseJSDialog(bool success,
                                             const std::string& response) {
  if (remote_.is_bound())
    remote_->CloseJSDialog(success, response);
}

void BrowserShellPageContents::ExecuteJavaScriptInAllFrames(
    const std::string& code) {
  if (remote_.is_bound())
    remote_->ExecuteJavaScriptInAllFrames(code);
}

void BrowserShellPageContents::ExecuteJavaScriptInMainFrame(
    const std::string& code) {
  if (remote_.is_bound())
    remote_->ExecuteJavaScriptInMainFrame(code);
}

bool BrowserShellPageContents::CanGoBack() const {
  return state_.can_go_back;
}

bool BrowserShellPageContents::CanGoForward() const {
  return state_.can_go_forward;
}

void BrowserShellPageContents::CloseNow() {
  remote_.reset();
}

std::string BrowserShellPageContents::GetUrl() const {
  return url_string_;
}

std::string BrowserShellPageContents::GetUserAgent() const {
  return user_agent_;
}

double BrowserShellPageContents::GetZoomFactor() const {
  return zoom_factor_;
}

void BrowserShellPageContents::GoBack() {
  if (remote_.is_bound() && state_.can_go_back)
    remote_->GoBack();
}

void BrowserShellPageContents::GoForward() {
  if (remote_.is_bound() && state_.can_go_forward)
    remote_->GoForward();
}

bool BrowserShellPageContents::IsAlive() const {
  return remote_.is_bound();
}

void BrowserShellPageContents::LoadFile(gin::Arguments* args) {
  if (!remote_.is_bound())
    return;

  if (is_main_contents_) {
    LOG(INFO) << "Reloading contents of main view is now unsupported!";
    return;
  }

  std::string file_path;
  if (!args->GetNext(&file_path))
    return;

  if (GURL(file_path).is_valid()) {
    LOG(INFO) << "Provided valid url instead of file path: " << file_path;
    remote_->LoadURL(file_path,
                     base::BindOnce(&BrowserShellPageContents::OnLoadURL,
                                    base::Unretained(this)));
    return;
  }

  std::string base_url("file://");
  if (!base::FilePath::FromUTF8Unsafe(file_path).IsAbsolute()) {
    v8::Isolate* isolate = args->isolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Object> location =
        global->Get(context, gin::StringToV8(isolate, "location"))
            .ToLocalChecked()->ToObject(context).ToLocalChecked();

    v8::Local<v8::String> href =
        location->Get(context, gin::StringToV8(isolate, "href"))
            .ToLocalChecked()->ToString(context).ToLocalChecked();
    base_url =
        GURL(gin::V8ToString(isolate, href)).GetWithoutFilename().spec();
  }

  std::string url = base::StrCat({base_url, file_path});
  remote_->LoadURL(url, base::BindOnce(&BrowserShellPageContents::OnLoadURL,
                                       base::Unretained(this)));
}

void BrowserShellPageContents::LoadURL(const std::string& url) {
  if (!remote_.is_bound())
    return;

  if (is_main_contents_) {
    LOG(INFO) << "Reloading contents of main view is now unsupported!";
    return;
  }

  if (!GURL(url).is_valid()) {
    LOG(INFO) << "Invalid url: " << url;
    return;
  }

  remote_->LoadURL(url, base::BindOnce(&BrowserShellPageContents::OnLoadURL,
                                       base::Unretained(this)));
}

void BrowserShellPageContents::Reload() {
  if (remote_.is_bound())
    remote_->Reload();
}

void BrowserShellPageContents::ResumeDOM() {
  if (!remote_.is_bound())
    return;

  if (is_main_contents_) {
    LOG(WARNING) << "The content of main BrowserShell application cannot"
                 << " be resumed because it can not be suspended.";
    return;
  }

  remote_->ResumeDOM();
}

void BrowserShellPageContents::ResumeMedia() {
  if (remote_.is_bound())
    remote_->ResumeMedia();
}

void BrowserShellPageContents::SetAcceptedLanguages(
    const std::string& languages) {
  if (remote_.is_bound())
    remote_->SetAcceptedLanguages(languages);
}

void BrowserShellPageContents::SetErrorPageHiding(bool enable) {
  if (!remote_.is_bound())
    return;

  if (error_page_hiding_ != enable) {
    remote_->SetErrorPageHiding(enable);
    error_page_hiding_ = enable;
  }
}

void BrowserShellPageContents::SetFocus() {
  if (remote_.is_bound())
    remote_->SetFocus();
}

void BrowserShellPageContents::SetPageBaseBackgroundColor(
    const std::string& color) {
  if (remote_.is_bound())
    remote_->SetPageBaseBackgroundColor(color);
}

void BrowserShellPageContents::SetSuppressSSLErrorPolicy(bool suppress_errors) {
  if (remote_.is_bound())
    remote_->SetSuppressSSLErrorPolicy(suppress_errors);
}

void BrowserShellPageContents::SetZoomFactor(double factor) {
  if (remote_.is_bound()) {
    zoom_factor_ = factor;
    remote_->SetZoomFactor(zoom_factor_);
  }
}

void BrowserShellPageContents::Stop() {
  if (remote_.is_bound())
    remote_->Stop();
}

void BrowserShellPageContents::SuspendDOM() {
  if (!remote_.is_bound())
    return;

  if (is_main_contents_) {
    LOG(WARNING) << "The content of main BrowserShell application cannot"
                 << " be suspended.";
    return;
  }

  remote_->SuspendDOM();
}

void BrowserShellPageContents::SuspendMedia() {
  if (remote_.is_bound())
    remote_->SuspendMedia();
}

void BrowserShellPageContents::OnCaptureVisibleRegionReply(
    std::unique_ptr<v8::Persistent<v8::Function>> callback,
    const std::string& base64_data) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper)) {
    LOG(ERROR) << __func__ << "(): can not get wrapper";
    return;
  }

  v8::Local<v8::Context> context;
  if (!wrapper->GetCreationContext().ToLocal(&context)) {
    LOG(ERROR) << __func__ << "(): can not get context";
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> local_callback = callback->Get(isolate);

  const int argc = 1;
  v8::Local<v8::Value> argv[] = {gin::StringToV8(isolate, base64_data)};
  std::ignore = local_callback->Call(context, wrapper, argc, argv);
}

gin::ObjectTemplateBuilder BrowserShellPageContents::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<BrowserShellPageContents>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kEmitMethodName, &BrowserShellPageContents::RunEmit)
      .SetMethod(kEventNamesMethodName,
                 &BrowserShellPageContents::RunGetEventNames)
      .SetMethod(kActivateMethodName, &BrowserShellPageContents::Activate)
      .SetMethod(kCaptureVisibleRegionMethodName,
                 &BrowserShellPageContents::CaptureVisibleRegion)
      .SetMethod(kClearDataMethodName, &BrowserShellPageContents::ClearData)
      .SetMethod(kCloseNowMethodName, &BrowserShellPageContents::CloseNow)
      .SetMethod(kDeactivateMethodName, &BrowserShellPageContents::Deactivate)
      .SetMethod(kListenerCountMethodName,
                 &BrowserShellPageContents::RunGetListenerCount)
      .SetMethod(kOnMethodName, &BrowserShellPageContents::RunAddEventListener)
      .SetMethod(kOnceMethodName,
                 &BrowserShellPageContents::RunAddOnceEventListener)
      .SetMethod(kRemoveEventListenerMethodName,
                 &BrowserShellPageContents::RunRemoveEventListener)
      .SetMethod(kRemoveAllEventListenersMethodName,
                 &BrowserShellPageContents::RunRemoveAllEventListeners)
      .SetMethod(kExecuteJavaScriptInAllFramesMethodName,
                 &BrowserShellPageContents::ExecuteJavaScriptInAllFrames)
      .SetMethod(kExecuteJavaScriptInMainFrameMethodName,
                 &BrowserShellPageContents::ExecuteJavaScriptInMainFrame)
      .SetMethod(kGoBackMethodName, &BrowserShellPageContents::GoBack)
      .SetMethod(kGoForwardMethodName, &BrowserShellPageContents::GoForward)
      .SetMethod(kLoadFileMethodName, &BrowserShellPageContents::LoadFile)
      .SetMethod(kLoadURLMethodName, &BrowserShellPageContents::LoadURL)
      .SetMethod(kReloadMethodName, &BrowserShellPageContents::Reload)
      .SetMethod(kResumeDOMMethodName, &BrowserShellPageContents::ResumeDOM)
      .SetMethod(kResumeMediaMethodName, &BrowserShellPageContents::ResumeMedia)
      .SetMethod(kSetAcceptedLanguagesMethodName,
                 &BrowserShellPageContents::SetAcceptedLanguages)
      .SetMethod(kSetFocusMethodName, &BrowserShellPageContents::SetFocus)
      .SetMethod(kSetPageBaseBackgroundColorMethodName,
                 &BrowserShellPageContents::SetPageBaseBackgroundColor)
      .SetMethod(kSetSuppressSSLErrorPolicyMethodName,
                 &BrowserShellPageContents::SetSuppressSSLErrorPolicy)
      .SetMethod(kStopMethodName, &BrowserShellPageContents::Stop)
      .SetMethod(kSuspendDOMMethodName, &BrowserShellPageContents::SuspendDOM)
      .SetMethod(kSuspendMediaMethodName,
                 &BrowserShellPageContents::SuspendMedia)
      .SetProperty(kIdPropertyName, &BrowserShellPageContents::GetID)
      .SetProperty(kCanGoBackPropertyName, &BrowserShellPageContents::CanGoBack)
      .SetProperty(kCanGoForwardPropertyName,
                   &BrowserShellPageContents::CanGoForward)
      .SetProperty(kErrorPageHidingPropertyName,
                   &BrowserShellPageContents::GetErrorPageHiding,
                   &BrowserShellPageContents::SetErrorPageHiding)
      .SetProperty(kIsActivePropertyName,
                   &BrowserShellPageContents::GetActiveState)
      .SetProperty(kIsAlivePropertyName, &BrowserShellPageContents::IsAlive)
      .SetProperty(kUrlPropertyName, &BrowserShellPageContents::GetUrl)
      .SetProperty(kUserAgentPropertyName,
                   &BrowserShellPageContents::GetUserAgent)
      .SetProperty(kZoomFactorProperyName,
                   &BrowserShellPageContents::GetZoomFactor,
                   &BrowserShellPageContents::SetZoomFactor);
}

}  // namespace injections
