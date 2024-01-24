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

#include "neva/app_runtime/app/app_runtime_page_contents.h"

#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/color_parser.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "neva/app_runtime/app/app_runtime_js_dialog_manager.h"
#include "neva/app_runtime/app/app_runtime_main_delegate.h"
#include "neva/app_runtime/app/app_runtime_page_contents_delegate.h"
#include "neva/app_runtime/app/app_runtime_page_view.h"
#include "neva/app_runtime/app/app_runtime_shell_environment.h"
#include "neva/app_runtime/app/app_runtime_visible_region_capture.h"
#include "neva/app_runtime/browser/app_runtime_browser_context.h"
#include "neva/app_runtime/browser/browsing_data/browsing_data_remover.h"
#include "neva/app_runtime/public/mojom/app_runtime_webview.mojom.h"
#include "neva/app_runtime/webapp_injection_manager.h"
#include "neva/logging.h"
#include "neva/user_agent/common/user_agent.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "url/gurl.h"

#if defined(USE_NEVA_MEDIA)
#include "content/public/browser/neva/media_state_manager.h"
#endif

namespace neva_app_runtime {
namespace {

bool IsStreamDevicesEmpty(const blink::mojom::StreamDevices& devices) {
  return !devices.audio_device.has_value() && !devices.video_device.has_value();
}

}  // namespace

PageContents::CreateParams::CreateParams() = default;

PageContents::CreateParams::CreateParams(const CreateParams&) = default;

PageContents::CreateParams&
PageContents::CreateParams::operator=(const CreateParams&) = default;

PageContents::CreateParams::~CreateParams() = default;

uint64_t PageContents::MediaAccessPermissionInfo::id = 0;

PageContents::MediaAccessPermissionInfo::MediaAccessPermissionInfo() = default;

PageContents::MediaAccessPermissionInfo::MediaAccessPermissionInfo(
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    content::MediaResponseCallback callback)
    : stream_devices_set_(stream_devices_set.Clone()),
      callback(std::move(callback)) {
  MediaAccessPermissionInfo::id++;
}

PageContents::MediaAccessPermissionInfo::MediaAccessPermissionInfo(
    PageContents::MediaAccessPermissionInfo&&) = default;
PageContents::MediaAccessPermissionInfo::~MediaAccessPermissionInfo() = default;

PageContents::PageContents(const CreateParams& params)
    : PageContents(CreateWebContents(params), params) {}

PageContents::~PageContents() {
  if (delegate_)
    delegate_->OnDestroying(this);

  if (web_contents_.get())
    web_contents_->SetDelegate(nullptr);

  ShellEnvironment::GetInstance()->Remove(this);
}

void PageContents::Activate() {
  if (!web_contents_) {
    web_contents_ =
        ReCreateWebContents(browser_context_, session_storage_namespace_map_);
    browser_context_ = nullptr;
    web_contents_->SetDelegate(this);
    Observe(web_contents_.get());
    SetUserAgentOverride(user_agent_);
    LoadURL(last_commited_url_);
  }
}

void PageContents::ClearData(const std::string& clear_options,
                             const std::string& clear_data_type_set) {
  if (!web_contents_)
    return;

  absl::optional<base::Value> options = base::JSONReader::Read(clear_options);
  BrowsingDataRemover::TimeRange delete_time_range =
      BrowsingDataRemover::Unbounded();
  if (options && options->is_dict()) {
    delete_time_range = BrowsingDataRemover::TimeRange(
        base::Time::FromDoubleT(options->GetDict().Find("since")->GetDouble()),
        base::Time::Now());
  }

  absl::optional<base::Value> data_type_set =
      base::JSONReader::Read(clear_data_type_set);
  uint32_t remove_data_mask = 0;
  if (data_type_set && data_type_set->is_dict()) {
    for (auto&& it : data_type_set->GetDict()) {
      if (it.second.GetBool()) {
        if (it.first == "cache") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_CACHE;
        } else if (it.first == "cookies") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_COOKIES;
        } else if (it.first == "sessionCookies") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_SESSION_COOKIES;
        } else if (it.first == "persistentCookies") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_PERSISTENT_COOKIES;
        } else if (it.first == "fileSystems") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_FILE_SYSTEMS;
        } else if (it.first == "indexedDB") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_INDEXEDDB;
        } else if (it.first == "localStorage") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_LOCAL_STORAGE;
        } else if (it.first == "webSQL") {
          remove_data_mask |= BrowsingDataRemover::RemoveBrowsingDataMask::
              REMOVE_WEBSQL;
        }
      }
    }
  }

  content::BrowserContext* browser_context = web_contents_->GetBrowserContext();
  content::StoragePartition* partition = browser_context->GetStoragePartition(
      web_contents_->GetSiteInstance(), false);
  if (partition) {
    BrowsingDataRemover* remover =
        BrowsingDataRemover::GetForStoragePartition(partition);
    remover->Remove(delete_time_range, remove_data_mask, GURL(),
                    base::DoNothing());
  }
}

void PageContents::CaptureVisibleRegion(const std::string& format,
                                        int quality) {
  if (!web_contents_)
    return;

  if (visible_region_capture_.get() == nullptr) {
    VisibleRegionCapture::ImageFormat image_format(
        VisibleRegionCapture::ImageFormat::kNone);
    if (format == "jpeg")
      image_format = VisibleRegionCapture::ImageFormat::kJpeg;
    else if (format == "png")
      image_format = VisibleRegionCapture::ImageFormat::kPng;

    if (image_format != VisibleRegionCapture::ImageFormat::kNone) {
      visible_region_capture_ = std::make_unique<VisibleRegionCapture>(
          base::BindOnce(&PageContents::OnCaptureVisibleRegion,
                         base::Unretained(this)),
          web_contents_.get(), image_format,
          std::max(std::min(100, quality), 0));
      return;
    }
  }
  delegate_->OnVisibleRegionCaptured(std::string());
}

void PageContents::Deactivate() {
  if (!web_contents_)
    return;

  // TODO(neva): Workaround to fix crash caused by deactivate suspended page.
  // Suspending of DOM stops pages incompletely. After the deletion of
  // web_contents_ there were free resources in blink related to this page,
  // but in renderer process have not stopped subframes, and in time of stopping that frames
  // FrameScheduler already not existed.
  ResumeDOM();

  content::RenderProcessHost* render_process =
      web_contents_->GetPrimaryMainFrame()->GetProcess();
  if (render_process)
    render_process->FastShutdownIfPossible(1);
  session_storage_namespace_map_ =
      web_contents_->GetController().GetSessionStorageNamespaceMap();
  last_commited_url_ = web_contents_->GetLastCommittedURL().spec();
  browser_context_ = web_contents_->GetBrowserContext();
  web_contents_.reset();
}

bool PageContents::IsActive() {
  return web_contents_.get() != nullptr;
}

void PageContents::SetDelegate(PageContentsDelegate* delegate) {
  delegate_ = delegate ? delegate : &stub_delegate_;
}

PageContentsDelegate* PageContents::GetDelegate() const {
  return (delegate_ == &stub_delegate_) ? nullptr : delegate_;
}

uint64_t PageContents::GetID() const {
  return id_;
}

content::WebContents* PageContents::GetWebContents() const {
  return web_contents_.get();
}

void PageContents::ExecuteJavaScriptInAllFrames(
    const std::string& code_string) {
  if (!web_contents_)
    return;

  const std::u16string js_code = base::UTF8ToUTF16(code_string);
  web_contents_->ForEachRenderFrameHost(
      [&js_code](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive())
          render_frame_host->ExecuteJavaScript(js_code, base::NullCallback());
      });
}

void PageContents::ExecuteJavaScriptInMainFrame(
    const std::string& code_string) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  if (rfh && rfh->IsRenderFrameLive()) {
    rfh->ExecuteJavaScriptForTests(base::UTF8ToUTF16(code_string),
                                   base::NullCallback());
  }
}

std::string PageContents::GetAcceptedLanguages() const {
  auto* renderer_prefs(web_contents_->GetMutableRendererPrefs());
  if (renderer_prefs)
    return renderer_prefs->accept_languages;
  return std::string();
}

bool PageContents::GetErrorPageHiding() const {
  return error_page_hiding_;
}

std::string PageContents::GetUserAgent() const {
  return user_agent_;
}

double PageContents::GetZoomFactor() const {
  return blink::PageZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(web_contents_.get()));
}

bool PageContents::CanGoBack() const {
  return web_contents_.get() ? web_contents_->GetController().CanGoBack()
                             : false;
}

bool PageContents::CanGoForward() const {
  return web_contents_.get() ? web_contents_->GetController().CanGoForward()
                             : false;
}

void PageContents::CloseJSDialog(bool success, const std::string& response) {
  if (js_dialog_manager_.get()) {
    js_dialog_manager_->CloseDialog(success, response);
  } else {
    LOG(WARNING) << __func__ << ", JavaScript dialog has never been created"
                 << " for this page contents.";
  }
}

void PageContents::GoBack() {
  if (web_contents_.get())
    web_contents_->GetController().GoBack();
}

void PageContents::GoForward() {
  if (web_contents_.get())
    web_contents_->GetController().GoForward();
}

bool PageContents::LoadURL(std::string url_string) {
  if (!web_contents_)
    return false;

  const GURL url(url_string);
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API);
  params.frame_name = std::string("");
  params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
  // todo: allow_local_resources_load_
  params.can_load_local_resources = true;
  web_contents_->GetController().LoadURLWithParams(params);
  RequestAllInjectionsLoading();
  return true;
}

bool PageContents::Reload() {
  if (!web_contents_)
    return false;

  web_contents_->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                        true);
  return true;
}

void PageContents::ReloadNoWarranty() {
  std::ignore = Reload();
}

void PageContents::ResumeDOM() {
  if (!web_contents_)
    return;

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->ResumeDOM();
  }
}

void PageContents::ResumeMedia() {
  if (!web_contents_)
    return;

#if defined(USE_NEVA_MEDIA)
  content::MediaStateManager::GetInstance()->ResumeAllMedia(
      web_contents_.get());
#else
  NOTIMPLEMENTED();
#endif
}

void PageContents::SetAcceptedLanguages(std::string languages) {
  if (!web_contents_)
    return;

  auto* renderer_prefs(web_contents_->GetMutableRendererPrefs());
  if (renderer_prefs &&
      renderer_prefs->accept_languages.compare(languages) != 0) {
    renderer_prefs->accept_languages = std::move(languages);
    web_contents_->SyncRendererPrefs();
    delegate_->OnAcceptedLanguagesChanged(renderer_prefs->accept_languages);
  }
}

void PageContents::SetErrorPageHiding(bool enable) {
  error_page_hiding_ = enable;
}

void PageContents::SetFocus() {
  if (!web_contents_)
    return;

  auto* renderer_host_view = web_contents_->GetRenderWidgetHostView();
  if (renderer_host_view)
    renderer_host_view->Focus();
}

void PageContents::SetPageBaseBackgroundColor(std::string color) {
  if (!web_contents_)
    return;

  SkColor parsed_color;
  if (!content::ParseHexColorString(color, &parsed_color))
    parsed_color = SK_ColorWHITE;
  web_contents_->SetPageBaseBackgroundColor(parsed_color);
}

void PageContents::SetZoomFactor(double factor) {
  if (!web_contents_)
    return;

  content::HostZoomMap::SetZoomLevel(web_contents_.get(),
                                     blink::PageZoomFactorToZoomLevel(factor));
}

void PageContents::Stop() {
  if (web_contents_.get())
    web_contents_->Stop();
}

void PageContents::SetUserAgentOverride(const std::string& user_agent) {
  if (user_agent.empty()) {
    user_agent_ = neva_user_agent::GetDefaultUserAgent();
  } else {
    user_agent_ = user_agent;
    if (web_contents_.get()) {
      auto user_agent_blink =
          blink::UserAgentOverride::UserAgentOnly(user_agent_);
      web_contents_->SetUserAgentOverride(user_agent_blink, false);
    }
  }
}

void PageContents::SetSSLCertErrorPolicy(SSLCertErrorPolicy policy) {
  ssl_cert_error_policy_ = policy;
}

void PageContents::SuspendDOM() {
  if (!web_contents_)
    return;

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->SuspendDOM();
  }
}

void PageContents::SuspendMedia() {
  if (!web_contents_)
    return;

#if defined(USE_NEVA_MEDIA)
  content::MediaStateManager::GetInstance()->SuspendAllMedia(
      web_contents_.get());
#else
  NOTIMPLEMENTED();
#endif
}

void PageContents::UpdatePreferredLanguage(std::string language) {
  if (!web_contents_)
    return;

  auto* renderer_prefs(web_contents_->GetMutableRendererPrefs());
  if (renderer_prefs) {
    // If the language was missing, add it to the beginning of accept_languages.
    // Otherwise move language to the beginning of the accept_languages string.
    std::vector<base::StringPiece> languages =
        base::SplitStringPiece(renderer_prefs->accept_languages, ",",
                               base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);

    if (!languages.empty() && languages.front().compare(language) == 0)
      return;

    std::vector<std::string> updated_languages;
    updated_languages.push_back(std::move(language));
    for (const base::StringPiece& lang : languages) {
      if (lang.compare(updated_languages.front()) != 0)
        updated_languages.push_back(std::string(lang));
    }

    renderer_prefs->accept_languages = base::JoinString(updated_languages, ",");
    web_contents_->SyncRendererPrefs();
    delegate_->OnAcceptedLanguagesChanged(renderer_prefs->accept_languages);
  }
}

PageView* PageContents::GetParentPageView() const {
  return parent_page_view_;
}

SSLCertErrorPolicy PageContents::GetSSLCertErrorPolicy() const {
  return ssl_cert_error_policy_;
}

void PageContents::DidFailLoad(content::RenderFrameHost*,
                               const GURL& validated_url,
                               int error_code) {
  delegate_->DidFailLoad(validated_url.spec(),
                         net::ErrorToShortString(error_code), error_code);
}

void PageContents::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                 const GURL& validated_url) {
  bool is_main_frame = !render_frame_host->GetParent();
  if (is_main_frame)
    delegate_->DidFinishLoad(validated_url.spec());
}

void PageContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsErrorPage()) {
    if (error_page_hiding_ && parent_page_view_)
      parent_page_view_->SetVisible(false, VisibilityChangeReason::kErrorPage);

    delegate_->DidFailLoad(
        navigation_handle->GetURL().spec(),
        net::ErrorToShortString(navigation_handle->GetNetErrorCode()),
        navigation_handle->GetNetErrorCode());
  }

  // Check for zoom changing when we navigate to different site
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameOrigin() &&
      !navigation_handle->IsSameDocument()) {
    delegate_->OnZoomFactorChanged(blink::PageZoomLevelToZoomFactor(
        host_zoom_map_->GetZoomLevelForHostAndScheme(
            std::string(), navigation_handle->GetURL().host())));
  }

  // Check for authentication request
  if (!navigation_handle->GetAuthChallengeInfo()) {
    auth_challenge_.reset();
    return;
  }

  auth_challenge_ = navigation_handle->GetAuthChallengeInfo();
  network_anonymization_key_ =
      navigation_handle->GetIsolationInfo().network_anonymization_key();

  neva_app_runtime::AuthChallengeInfo sending_challenge_info;

  sending_challenge_info.is_proxy = auth_challenge_.value().is_proxy;
  sending_challenge_info.port = auth_challenge_.value().challenger.port();
  sending_challenge_info.url = auth_challenge_.value().challenger.GetURL().spec();
  sending_challenge_info.scheme = auth_challenge_.value().scheme;
  sending_challenge_info.host = auth_challenge_.value().challenger.host();
  sending_challenge_info.realm = auth_challenge_.value().realm;

  delegate_->OnAuthChallenge(sending_challenge_info);

  if (auth_challenge_.value().is_proxy) {
    navigation_handle->GetWebContents()->DidChangeVisibleSecurityState();
  }
}

void PageContents::DidStartLoading() {
  delegate_->DidStartLoading();
}

void PageContents::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle)
    delegate_->DidStartNavigation(navigation_handle->GetURL().spec());
}

void PageContents::DidStopLoading() {
  delegate_->DidStopLoading();
}

void PageContents::DidUpdateFaviconURL(
    content::RenderFrameHost*,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  std::vector<FaviconInfo> sending_info;
  sending_info.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    FaviconInfo info;
    info.url = candidate->icon_url.spec();
    info.type = ContentIconTypeToString(candidate->icon_type);
    for (const auto& icon_size : candidate->icon_sizes) {
      FaviconSize favicon_size;
      favicon_size.width = icon_size.width();
      favicon_size.height = icon_size.height();
      info.sizes.push_back(std::move(favicon_size));
    }
    sending_info.push_back(std::move(info));
  }

  delegate_->DidUpdateFaviconUrl(sending_info);
}

void PageContents::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent())
    delegate_->DOMReady();
}

void PageContents::LoadProgressChanged(double progress) {
  const unsigned int percent =
      static_cast<unsigned int>(std::ceil(progress*100.));
  delegate_->LoadProgressChanged(percent);
}

void PageContents::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  delegate_->NavigationEntryCommitted();
}

void PageContents::OnRendererUnresponsive(
    content::RenderProcessHost* render_process_host) {
  delegate_->OnRendererUnresponsive();
}

void PageContents::OnRendererResponsive(
    content::RenderProcessHost* render_process_host){
  delegate_->OnRendererResponsive();
}

void PageContents::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  delegate_->OnFocusChanged(true);
}

void PageContents::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  delegate_->OnFocusChanged(false);
}

void PageContents::AckPermission(bool ack, uint64_t id) {
  auto find_info = media_access_requests_.find(id);
  if (find_info == media_access_requests_.end()) {
    LOG(WARNING) << __func__ << " Not found request";
    return;
  }

  MediaAccessPermissionInfo info = std::move(find_info->second);
  media_access_requests_.erase(find_info);
  blink::mojom::MediaStreamRequestResult result;
  if (ack) {
    result =
        IsStreamDevicesEmpty(*(info.stream_devices_set_->stream_devices[0]))
            ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
            : blink::mojom::MediaStreamRequestResult::OK;
  } else {
    result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  }

  std::move(info.callback).Run(*info.stream_devices_set_, result, nullptr);
}

void PageContents::AckAuthChallenge(const std::string& login,
                                    const std::string& passwd,
                                    const std::string& url) {
  if (!web_contents_)
    return;

  if (!auth_challenge_ ||
      (auth_challenge_.value().challenger.GetURL().spec() != url))
    return;

  net::AuthCredentials credentials(base::UTF8ToUTF16(login),
                                   base::UTF8ToUTF16(passwd));

  content::StoragePartition* storage_partition =
      web_contents_->GetBrowserContext()->GetStoragePartition(
          web_contents_->GetSiteInstance());

  storage_partition->GetNetworkContext()->AddAuthCacheEntry(
      auth_challenge_.value(), network_anonymization_key_, credentials,
      base::BindOnce(&PageContents::ReloadNoWarranty,
                     weak_ptr_factory_.GetWeakPtr()));
  auth_challenge_.reset();

  if (credentials.Empty())
    Reload();
}

void PageContents::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (callback.is_null())
    return;

  blink::mojom::StreamDevicesSet devices_set;
  devices_set.stream_devices.emplace_back(blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *devices_set.stream_devices[0];

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    devices.audio_device = blink::MediaStreamDevice(
        request.audio_type, request.requested_audio_device_id, "");
  }
  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    devices.video_device = blink::MediaStreamDevice(
        request.video_type, request.requested_video_device_id, "");
  }

  if (IsStreamDevicesEmpty(devices)) {
    std::move(callback).Run(devices_set,
                            blink::mojom::MediaStreamRequestResult::NO_HARDWARE,
                            nullptr);
    return;
  }

  // If we have default values no need emit request further.
  if (default_access_to_media_) {
    auto request_result =
        default_access_to_media_.value()
            ? blink::mojom::MediaStreamRequestResult::OK
            : blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;

    std::move(callback).Run(devices_set, request_result, nullptr);
    return;
  }

  MediaAccessPermissionInfo permission_info(devices_set, std::move(callback));
  media_access_requests_.insert(
      {MediaAccessPermissionInfo::id, std::move(permission_info)});

  delegate_->OnPermissionRequest("media", MediaAccessPermissionInfo::id);
}

void PageContents::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  std::string reason;
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      reason = "normal";
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      reason = "abnormal";
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      reason = "crash";
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      reason = "kill";
      break;
    default:
      reason = "unexpected";
      break;
  }
  delegate_->OnExit(reason);
}

void PageContents::RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                          content::RenderFrameHost* new_host) {
  // If our associated HostZoomMap changes, update our subscription.
  content::HostZoomMap* new_host_zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents_.get());
  if (new_host_zoom_map == host_zoom_map_)
    return;

  host_zoom_map_ = new_host_zoom_map;
  zoom_changed_subscription_ =
      host_zoom_map_->AddZoomLevelChangedCallback(base::BindRepeating(
          &PageContents::OnZoomLevelChanged, base::Unretained(this)));
}

void PageContents::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  NewWindowInfo window_info;
  window_info.target_url = target_url.spec();
  window_info.initial_width = window_features.bounds.width();
  window_info.initial_height = window_features.bounds.height();
  if (new_contents->GetPrimaryMainFrame())
    window_info.name = new_contents->GetPrimaryMainFrame()->GetFrameName();

  switch (disposition) {
    case WindowOpenDisposition::IGNORE_ACTION:
      window_info.window_open_disposition = "ignore";
      break;
    case WindowOpenDisposition::SAVE_TO_DISK:
      window_info.window_open_disposition = "save_to_disk";
      break;
    case WindowOpenDisposition::CURRENT_TAB:
      window_info.window_open_disposition = "current_tab";
      break;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      window_info.window_open_disposition = "new_background_tab";
      break;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      window_info.window_open_disposition = "new_foreground_tab";
      break;
    case WindowOpenDisposition::NEW_WINDOW:
      window_info.window_open_disposition = "new_window";
      break;
    case WindowOpenDisposition::NEW_POPUP:
      window_info.window_open_disposition = "new_popup";
      break;
    default:
      break;
  }

  CreateParams params;
  params.width = window_features.bounds.width();
  params.height = window_features.bounds.height();
  params.user_agent = user_agent_;
  params.error_page_hiding = error_page_hiding_;
  params.default_access_to_media = default_access_to_media_;

  if (was_blocked)
    *was_blocked = false;

  delegate_->OnNewWindowOpen(
      std::unique_ptr<PageContents>(
          new PageContents(std::move(new_contents), params)),
      window_info);
}

void PageContents::CloseContents(content::WebContents* source) {
  delegate_->OnClose();
}

// static
std::unique_ptr<content::WebContents> PageContents::CreateWebContents(
    const PageContents::CreateParams& params) {
  AppRuntimeBrowserContext* browser_context;
  browser_context =
      AppRuntimeBrowserContext::From(params.storage_partition_name,
                                     params.storage_partition_off_the_record);
  content::WebContents::CreateParams contents_params(browser_context, nullptr);
  return content::WebContents::Create(contents_params);
}

// static
std::unique_ptr<content::WebContents> PageContents::ReCreateWebContents(
    content::BrowserContext* browser_context,
    const content::SessionStorageNamespaceMap& session_storage_namespace_map) {
  NEVA_DCHECK(browser_context != nullptr);
  content::WebContents::CreateParams contents_params(browser_context, nullptr);
  return content::WebContents::CreateWithSessionStorage(
      contents_params, session_storage_namespace_map);
}

PageContents::PageContents(std::unique_ptr<content::WebContents> new_contents,
                           const CreateParams& params)
    : id_(ShellEnvironment::GetInstance()->GetNextIDFor(this)),
      delegate_(params.delegate ? params.delegate : &stub_delegate_),
      web_contents_(std::move(new_contents)),
      default_access_to_media_(params.default_access_to_media),
      injections_(params.injections),
      injection_manager_(std::make_unique<WebAppInjectionManager>()),
      error_page_hiding_(params.error_page_hiding) {
  if (!web_contents_)
    return;

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());
  SetUserAgentOverride(params.user_agent);
  SetWebPreferences(params);
  SetRendererPreferences(params);

  host_zoom_map_ = content::HostZoomMap::GetForWebContents(web_contents_.get());
  zoom_changed_subscription_ =
      host_zoom_map_->AddZoomLevelChangedCallback(base::BindRepeating(
          &PageContents::OnZoomLevelChanged, base::Unretained(this)));
}

void PageContents::SetParentPageView(PageView* page_view) {
  NEVA_DCHECK(!page_view || !parent_page_view_);
  parent_page_view_ = page_view;
  if (parent_page_view_)
    Activate();
  else
    Deactivate();
}

void PageContents::SetRendererPreferences(
    const PageContents::CreateParams& params) {
  if (!web_contents_)
    return;

  auto* renderer_prefs(web_contents_->GetMutableRendererPrefs());
  if (!renderer_prefs)
    return;

  if (!params.app_id.empty() &&
      renderer_prefs->application_id.compare(params.app_id) != 0)
    renderer_prefs->application_id = params.app_id;

  web_contents_->SyncRendererPrefs();
}

void PageContents::SetWebPreferences(const PageContents::CreateParams& params) {
  if (!web_contents_)
    return;

  blink::web_pref::WebPreferences web_preferences(
      web_contents_->GetOrCreateWebPreferences());
  web_preferences.allow_file_access_from_file_urls =
      params.allow_file_access_from_file_urls;
  web_preferences.allow_universal_access_from_file_urls =
      params.allow_universal_access_from_file_urls;
  web_contents_->SetWebPreferences(web_preferences);
}

void PageContents::RequestAllInjectionsLoading() {
  if (!web_contents_)
    return;

  for (const auto& name : injections_) {
    injection_manager_->RequestLoadInjection(
        web_contents_->GetPrimaryMainFrame(), name);
  }
}

void PageContents::OnCaptureVisibleRegion(std::string base64_data) {
  visible_region_capture_.reset();
  delegate_->OnVisibleRegionCaptured(base64_data);
}

void PageContents::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  if (!web_contents_)
    return;

  if (net::GetHostOrSpecFromURL(web_contents_->GetLastCommittedURL()) ==
      change.host) {
    delegate_->OnZoomFactorChanged(
        blink::PageZoomLevelToZoomFactor(change.zoom_level));
  }
}

content::JavaScriptDialogManager* PageContents::GetJavaScriptDialogManager(
    content::WebContents* source) {
  if (!js_dialog_manager_.get())
    js_dialog_manager_ = std::make_unique<JSDialogManager>(this);
  return js_dialog_manager_.get();
}

void PageContents::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  page_requested_fullscreen_ = true;
  delegate_->EnterHtmlFullscreen();
}

void PageContents::FullscreenStateChangedForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {}

void PageContents::ExitFullscreenModeForTab(content::WebContents*) {
  delegate_->LeaveHtmlFullscreen();
  page_requested_fullscreen_ = false;
}

bool PageContents::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return page_requested_fullscreen_;
}

void PageContents::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (content::INVALIDATE_TYPE_TITLE & changed_flags) {
    const std::string title = base::UTF16ToUTF8(source->GetTitle());
    delegate_->TitleUpdated(title);
  }
}

bool PageContents::RunJSDialog(const std::string& type,
                               const std::string& message) {
  return delegate_->RunJSDialog(type, message);
}

}  // namespace neva_app_runtime
