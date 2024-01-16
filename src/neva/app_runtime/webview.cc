// Copyright 2016 LG Electronics, Inc.
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

#include "neva/app_runtime/webview.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging_category.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/switches.h"
#include "components/media_control/browser/neva/media_suspender.h"
#include "components/permissions/permission_request_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "neva/app_drm/appdrm_common.h"
#include "neva/app_runtime/app/app_runtime_main_delegate.h"
#include "neva/app_runtime/browser/app_runtime_browser_context.h"
#include "neva/app_runtime/browser/app_runtime_browser_switches.h"
#include "neva/app_runtime/browser/app_runtime_webview_controller_impl.h"
#include "neva/app_runtime/browser/app_runtime_webview_host_impl.h"
#include "neva/app_runtime/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "neva/app_runtime/public/app_runtime_event.h"
#include "neva/app_runtime/public/webview_delegate.h"
#include "neva/app_runtime/webapp_injection_manager.h"
#include "neva/app_runtime/webview_profile.h"
#include "neva/logging.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/logging/logging_utils.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/font_render_params.h"

#if defined(USE_NEVA_MEDIA)
#include "content/public/browser/neva/media_state_manager.h"
#include "media/base/media_switches_neva.h"
#endif

#if defined(ENABLE_PLUGINS)
void GetPluginsCallback(const std::vector<content::WebPluginInfo>& plugins) {}
#endif

namespace {

void AddUserStyleSheetForFrame(const std::string& sheet,
                               content::RenderFrameHost* rfh) {
  mojo::AssociatedRemote<neva_app_runtime::mojom::AppRuntimeWebViewClient>
      client;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->InsertStyleSheet(sheet);
}

}  // namespace

namespace neva_app_runtime {
namespace {

bool ConvertVisibilityState(WebPageVisibilityState from,
                            neva_app_runtime::mojom::VisibilityState& to) {
  switch (from) {
    case WebPageVisibilityStateVisible:
      to = neva_app_runtime::mojom::VisibilityState::kVisible;
      break;
    case WebPageVisibilityStateHidden:
      to = neva_app_runtime::mojom::VisibilityState::kHidden;
      break;
    case WebPageVisibilityStateLaunching:
      to = neva_app_runtime::mojom::VisibilityState::kLaunching;
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

void WebView::SetFileAccessBlocked(bool blocked) {
  NOTIMPLEMENTED();
}

WebView::WebView(int width, int height, WebViewProfile* profile)
    : width_(width),
      height_(height),
      profile_(profile ? profile : WebViewProfile::GetDefaultProfile()) {
  TRACE_EVENT2("neva", "WebView::WebView", "width", width, "height", height);
  CreateWebContents();
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  use_aggressive_release_policy_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableAggressiveReleasePolicy);

  // Default policy : Skip frame is enabled.
  SetSkipFrame(true);

  web_contents_->SyncRendererPrefs();
  web_preferences_.reset(
      new blink::web_pref::WebPreferences(
          web_contents_->GetOrCreateWebPreferences()));

  web_contents_->SetInspectablePage(false);

  if (base::CommandLine::ForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    UpdatePreferencesAttribute(Attribute::WebSecurityEnabled, false);
  }
}

WebView::~WebView() {
  SetCorsCorbDisabled(false);
  web_contents_->SetDelegate(nullptr);
}

void WebView::CreateRenderView() {
  if (web_contents_) {
    // This code ensures that renderer proccess will be created before the first
    // neva_app_runtime::WebView API call which relies on fact that
    // renderer process has been already created and initialized
    if (!web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
      content::WebContentsImpl* webcontents_impl =
          static_cast<content::WebContentsImpl*>(web_contents_.get());
      content::RenderViewHost* rvh =
          web_contents_->GetPrimaryMainFrame()->GetRenderViewHost();
      webcontents_impl->CreateRenderViewForRenderManager(rvh, absl::nullopt,
                                                         nullptr);
    }
  }
}

void WebView::SetDelegate(WebViewDelegate* delegate) {
  webview_delegate_ = delegate;
  auto* webview_host_impl =
      AppRuntimeWebViewHostImpl::FromWebContents(web_contents_.get());
  if (webview_host_impl)
    webview_host_impl->SetDelegate(delegate);
}

void WebView::SetControllerDelegate(WebViewControllerDelegate* delegate) {
  auto* webview_controller_impl =
      AppRuntimeWebViewControllerImpl::FromWebContents(web_contents_.get());
  if (webview_controller_impl)
    webview_controller_impl->SetDelegate(delegate);
}

void WebView::CreateWebContents() {
  TRACE_EVENT0("neva", "WebView::CreateWebContents");

  content::BrowserContext* browser_context = profile_->GetBrowserContext();
  content::WebContents::CreateParams params(browser_context, nullptr);
  params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  web_contents_ = content::WebContents::Create(params);
  injection_manager_ = std::make_unique<WebAppInjectionManager>();
  AppRuntimeWebViewHostImpl::CreateForWebContents(web_contents_.get());
  AppRuntimeWebViewControllerImpl::CreateForWebContents(web_contents_.get());

  permissions::PermissionRequestManager::CreateForWebContents(
      web_contents_.get());
}

content::WebContents* WebView::GetWebContents() {
  return web_contents_.get();
}

void WebView::AddUserStyleSheet(const std::string& sheet) {
  TRACE_EVENT1("neva", "WebView::AddUserStyleSheet", "sheet length",
               sheet.length());

  web_contents_->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&sheet](content::RenderFrameHost* rfh) {
        AddUserStyleSheetForFrame(sheet, rfh);
      });
  injected_css_.insert(sheet);
}

std::string WebView::UserAgent() const {
  TRACE_EVENT0("neva", "WebView::UserAgent");

  return web_contents_->GetUserAgentOverride().ua_string_override;
}

void WebView::LoadUrl(const GURL& url) {
  TRACE_EVENT1("neva", "WebView::LoadUrl", "url", url.spec());

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API);
  params.frame_name = std::string("");
  params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
  params.can_load_local_resources = allow_local_resources_load_;
  web_contents_->GetController().LoadURLWithParams(params);
}

void WebView::StopLoading() {
  TRACE_EVENT0("neva", "WebView::StopLoading");

  int index = web_contents_->GetController().GetPendingEntryIndex();
  if (index != -1)
    web_contents_->GetController().RemoveEntryAtIndex(index);

  web_contents_->Stop();
  web_contents_->Focus();
}

void WebView::LoadExtension(const std::string& name) {
  TRACE_EVENT1("neva", "WebView::LoadExtension", "name", name);

  RequestInjectionLoading(name);
}

void WebView::ClearExtensions() {
  TRACE_EVENT0("neva", "WebView::ClearExtensions");

  RequestClearInjections();
}

void WebView::ReplaceBaseURL(const std::string& new_url) {
  TRACE_EVENT1("neva", "WebView::ReplaceBaseURL", "new_url", new_url);

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->ReplaceBaseURL(new_url);
  }
}

void WebView::SendWebViewInfo(const std::string& app_path,
                              const std::string& trust_level) {
  TRACE_EVENT2("neva", "WebView::SendWebViewInfo", "app_path", app_path,
               "trust_level", trust_level);

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<neva_app_runtime::mojom::AppRuntimeWebViewClient>
        client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWebViewInfo(app_path, trust_level);
  }
}

const std::string& WebView::GetUrl() {
  return web_contents_->GetVisibleURL().spec();
}

void WebView::SuspendDOM() {
  TRACE_EVENT0("neva", "WebView::SuspendDOM");

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->SuspendDOM();
  }
}

void WebView::ResumeDOM() {
  TRACE_EVENT0("neva", "WebView::ResumeDOM");

  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->ResumeDOM();
  }
}

void WebView::SuspendMedia() {
  TRACE_EVENT0("neva", "WebView::SuspendMedia");

#if defined(USE_NEVA_MEDIA)
  content::MediaStateManager::GetInstance()->SuspendAllMedia(
      web_contents_.get());
#endif
}

void WebView::ResumeMedia() {
  TRACE_EVENT0("neva", "WebView::ResumeMedia");

#if defined(USE_NEVA_MEDIA)
  content::MediaStateManager::GetInstance()->ResumeAllMedia(
      web_contents_.get());
#endif
}

void WebView::SuspendPaintingAndSetVisibilityHidden() {
  TRACE_EVENT0("neva", "WebView::SuspendPaintingAndSetVisibilityHidden");

  content::RenderWidgetHostViewAura* const host_view =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents_->GetRenderViewHost()->GetWidget()->GetView());
  if (host_view) {
    host_view->Hide();
    if (use_aggressive_release_policy_)
      host_view->SuspendDrawing();
  }
}

void WebView::ResumePaintingAndSetVisibilityVisible() {
  TRACE_EVENT0("neva", "WebView::ResumePaintingAndSetVisibilityVisible");

  content::RenderWidgetHostViewAura* const host_view =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents_->GetRenderViewHost()->GetWidget()->GetView());
  if (host_view) {
    host_view->Show();
    if (use_aggressive_release_policy_)
      host_view->ResumeDrawing();
  }
}

bool WebView::SetSkipFrame(bool enable) {
  NOTIMPLEMENTED();
  return true;
}

void WebView::CommitLoadVisually() {
  NOTIMPLEMENTED();
}

std::string WebView::DocumentTitle() const {
  return document_title_;
}

void WebView::RunJavaScript(const std::string& js_code) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  if (rfh && rfh->IsRenderFrameLive())
    rfh->ExecuteJavaScript(base::UTF8ToUTF16(js_code), base::NullCallback());
  else
    LOG(ERROR) << __FUNCTION__ << "(): RenderFrameHost check failed!";
}

void WebView::RunJavaScriptInAllFrames(const std::string& js_code) {
  TRACE_EVENT1("neva", "WebView::RunJavaScriptInAllFrames", "js_code length",
               js_code.length());

  web_contents_->ForEachRenderFrameHost(
      [&js_code](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive()) {
          render_frame_host->ExecuteJavaScript(base::UTF8ToUTF16(js_code),
                                               base::NullCallback());
        }
      });
}

void WebView::Reload() {
  TRACE_EVENT0("neva", "WebView::Reload");

  web_contents_->GetController().Reload(content::ReloadType::NONE, false);
  web_contents_->Focus();
}

int WebView::RenderProcessPid() const {
  content::RenderProcessHost* host = web_contents_->GetPrimaryMainFrame()->GetProcess();
  if (host)
    return host->GetProcess().Handle();
  return -1;
}

bool WebView::IsDrmEncrypted(const std::string& url) {
  app_drm::AppDRMFileManagerWrapper adfm =
      app_drm::AppDRMFileManagerWrapper(url);
  return adfm.Check();
}

std::string WebView::DecryptDrm(const std::string& url) {
  TRACE_EVENT1("neva", "WebView::DecryptDrm", "url", url);

  app_drm::AppDRMFileManagerWrapper adfm =
      app_drm::AppDRMFileManagerWrapper(url);
  return adfm.Decrypt();
}

int WebView::DevToolsPort() const {
  return static_cast<AppRuntimeBrowserMainParts*>(
      GetAppRuntimeContentBrowserClient()->GetMainParts())
          ->DevToolsPort();
}

void WebView::SetInspectable(bool enable) {
  TRACE_EVENT1("neva", "WebView::SetInspectable", "enable", enable);

  AppRuntimeBrowserMainParts* mp =
    static_cast<AppRuntimeBrowserMainParts*>(
      GetAppRuntimeContentBrowserClient()->GetMainParts());

  if (enable)
    mp->EnableDevTools();
  else
    mp->DisableDevTools();
}

void WebView::AddAvailablePluginDir(const std::string& directory) {
  TRACE_EVENT1("neva", "WebView::AddAvailablePluginDir", "directory",
               directory);
  NOTIMPLEMENTED();
}

void WebView::AddCustomPluginDir(const std::string& directory) {
  NOTIMPLEMENTED();
}

void WebView::SetBackgroundColor(int r, int g, int b, int a) {
  TRACE_EVENT0("neva", "WebView::SetBackgroundColor");

  web_contents_->SetPageBaseBackgroundColor(
      absl::make_optional(SkColorSetARGB(a, r, g, b)));
}

void WebView::SetAllowFakeBoldText(bool allow) {
  TRACE_EVENT1("neva", "WebView::SetAllowFakeBoldText", "allow", allow);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (renderer_prefs->allow_fake_bold_text == allow)
    return;

  renderer_prefs->allow_fake_bold_text = allow;

  web_contents_->SyncRendererPrefs();
}

void WebView::LoadProgressChanged(double progress) {
  if (webview_delegate_)
    webview_delegate_->OnLoadProgressChanged(progress);
}

// OpenURLFromTab() method is implemented for transition from old_url to new_url
// where old_url.SchemeIs(url::kFileScheme) == false
// and   new_url.SchemeIs(url::kFileScheme) == true
// Please see RenderFrameImpl::BeginNavigation() for "should_fork".
// If "should_fork == true" then we come here
content::WebContents* WebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  TRACE_EVENT0("neva", "WebView::OpenURLFromTab");

  if (!source) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  if (params.disposition != WindowOpenDisposition::CURRENT_TAB) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  return source;
}

void WebView::NavigationStateChanged(content::WebContents* source,
                                     content::InvalidateTypes changed_flags) {
  if (content::INVALIDATE_TYPE_TITLE & changed_flags) {
    document_title_ = base::UTF16ToUTF8(source->GetTitle());
    if (webview_delegate_)
      webview_delegate_->TitleChanged(document_title_);
  }
}

void WebView::CloseContents(content::WebContents* source) {
  TRACE_EVENT0("neva", "WebView::CloseContents");

  if (webview_delegate_)
    webview_delegate_->Close();
}

bool WebView::ShouldSuppressDialogs(content::WebContents* source) {
  return should_suppress_dialogs_;
}

bool WebView::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  logging::LogSeverity resolved_level =
      blink::ConsoleMessageLevelToLogSeverity(log_level);

  if (::logging::GetMinLogLevel() > resolved_level)
    return false;

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();

  std::string app_id;
  if (renderer_prefs)
    app_id = renderer_prefs->application_id;

  logging::CategoryLogMessage("CONSOLE", line_no, resolved_level,
                              logging::LOG_CATEGORY_JSCONSOLE)
          .stream()
      << app_id << (app_id.empty() ? "" : " ") << "\"" << message
      << "\", source: " << source_id << " (" << line_no << ")";

  return true;
}

void WebView::SetShouldSuppressDialogs(bool suppress) {
  TRACE_EVENT1("neva", "WebView::SetShouldSuppressDialogs", "suppress",
               suppress);

  should_suppress_dialogs_ = suppress;
}

void WebView::SetAppId(const std::string& app_id) {
  TRACE_EVENT1("neva", "WebView::SetAppId", "app_id", app_id);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();

  // app_id = application name + display affinity
  // umediaserver needs application name for acg and display affinity
  // for video to play on multiple diplays
  // Newly introduced local storage manager uses application_id,
  // so we need to provide the name without display affinity.
  // [FIXME] Make clear to use unique key like instance id for OSE
  // to get application name, display affinity
  int pos = app_id.size() - 1;
  std::string application_id = app_id.substr(0, pos);
  std::string display_id = app_id.substr(pos, app_id.size());
  if (!renderer_prefs->application_id.compare(application_id) &&
      !renderer_prefs->display_id.compare(display_id))
    return;
  renderer_prefs->application_id = application_id;
  renderer_prefs->display_id = display_id;
  renderer_prefs->is_enact_browser = false;

  web_contents_->SyncRendererPrefs();
}

void WebView::SetSecurityOrigin(const std::string& identifier) {
  TRACE_EVENT1("neva", "WebView::SetSecurityOrigin", "identifier", identifier);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  NEVA_DCHECK(!command_line->HasSwitch(switches::kProcessPerSite) &&
              !command_line->HasSwitch(switches::kProcessPerTab) &&
              !command_line->HasSwitch(switches::kSingleProcess))
      << "Wrong process model for calling WebView::SetSecurityOrigin() method!";

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (!renderer_prefs->file_security_origin.compare(identifier))
    return;

  renderer_prefs->file_security_origin = identifier;

  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  if (rvh) {
    GURL url = url::SchemeHostPort(url::kFileScheme, identifier, 0).GetURL();
    content::ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
        rvh->GetProcess()->GetID(), url);
  }

  web_contents_->SyncRendererPrefs();

  // Set changed origin mode for browser process
  if (!identifier.empty())
    url::Origin::SetFileOriginChanged(true);
}

void WebView::SetAcceptLanguages(const std::string& languages) {
  TRACE_EVENT1("neva", "WebView::SetAcceptLanguages", "languages", languages);

  auto* rendererPrefs(web_contents_->GetMutableRendererPrefs());
  if (!rendererPrefs->accept_languages.compare(languages))
    return;

  rendererPrefs->accept_languages = languages;

  web_contents_->SyncRendererPrefs();

  std::vector<std::string> locales = base::SplitString(
      languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (locales.size() > 0) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    web_contents_->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces()->
        GetInterface(&client);
    client->ChangeLocale(locales[0]);
  }

  content::BrowserContext* browser_context = profile_->GetBrowserContext();
  content::StoragePartition* storage_partition =
      browser_context->GetStoragePartition(nullptr);
  storage_partition->GetNetworkContext()->SetAcceptLanguage(
      net::HttpUtil::ExpandLanguageList(languages));
}

void WebView::SetUseLaunchOptimization(bool enabled, int delay_ms) {
  NOTIMPLEMENTED();
}

void WebView::SetUseEnyoOptimization(bool enabled) {
  NOTIMPLEMENTED();
  // TODO(jose.dapena): patch not ported
}

void WebView::SetBlockWriteDiskcache(bool blocked) {
  NOTIMPLEMENTED();
  // TODO(jose.dapena): patch not ported
}

void WebView::SetTransparentBackground(bool enable) {
  TRACE_EVENT1("neva", "WebView::SetTransparentBackground", "enable", enable);

  if (enable)
    SetBackgroundColor(0, 0, 0, 0);
}

void WebView::SetBoardType(const std::string& board_type) {
  TRACE_EVENT1("neva", "WebView::SetBoardType", "board_type", board_type);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (!renderer_prefs->board_type.compare(board_type))
    return;

  renderer_prefs->board_type = board_type;

  web_contents_->SyncRendererPrefs();
}

void WebView::SetMediaCodecCapability(const std::string& capability) {
  TRACE_EVENT1("neva", "WebView::SetMediaCodecCapability", "capability",
               capability);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (!renderer_prefs->media_codec_capability.compare(capability))
    return;

  renderer_prefs->media_codec_capability = capability;

  web_contents_->SyncRendererPrefs();
}

void WebView::SetMediaPreferences(const std::string& preferences) {
  TRACE_EVENT1("neva", "WebView::SetMediaPreferences", "preferences",
               preferences);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (!renderer_prefs->media_preferences.compare(preferences))
    return;

  renderer_prefs->media_preferences = preferences;

  web_contents_->SyncRendererPrefs();
}

void WebView::SetSearchKeywordForCustomPlayer(bool enabled) {
  NOTIMPLEMENTED();
  // TODO(jose.dapena): patch not ported
}

void WebView::SetUseVideoDecodeAccelerator(bool enable) {
#if defined(USE_NEVA_MEDIA)
  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<neva_app_runtime::mojom::AppRuntimeWebViewClient>
        client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetUseVideoDecodeAccelerator(enable);
  }
#endif
}

void WebView::SetUseUnlimitedMediaPolicy(bool enabled) {
  TRACE_EVENT1("neva", "WebView::SetUseUnlimitedMediaPolicy", "enabled",
               enabled);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();

  if (renderer_prefs->use_unlimited_media_policy == enabled)
    return;

  renderer_prefs->use_unlimited_media_policy = enabled;

  web_contents_->SyncRendererPrefs();
}

void WebView::SetEnableBackgroundRun(bool enabled) {
#if defined(USE_NEVA_MEDIA)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebMediaPlayerNeva)) {
    return;
  }
#endif
  auto* webview_controller_impl =
      AppRuntimeWebViewControllerImpl::FromWebContents(web_contents_.get());
  if (webview_controller_impl)
    webview_controller_impl->SetBackgroundVideoPlaybackEnabled(enabled);
}

void WebView::UpdatePreferencesAttributeForPrefs(
    blink::web_pref::WebPreferences* preferences,
    WebView::Attribute attribute,
    bool enable) {
  switch (attribute) {
    case Attribute::AllowRunningInsecureContent:
      preferences->allow_running_insecure_content = enable;
      break;
    case Attribute::AllowScriptsToCloseWindows:
      preferences->allow_scripts_to_close_windows = enable;
      break;
    case Attribute::AllowUniversalAccessFromFileUrls:
      preferences->allow_universal_access_from_file_urls = enable;
      break;
    case Attribute::BackHistoryKeyDisabled: {
      auto* webview_host_impl =
          AppRuntimeWebViewHostImpl::FromWebContents(web_contents_.get());
      if (webview_host_impl)
        webview_host_impl->SetBackHistoryKeyDisabled(enable);
    } break;
    case Attribute::SuppressesIncrementalRendering:
      NOTIMPLEMENTED()
          << "Attribute::SuppressesIncrementalRendering is not supported";
      break;
    case Attribute::DisallowScrollbarsInMainFrame:
      SetDisallowScrollbarsInMainFrame(enable);
      break;
    // According commit 5c434bb2 : Remove obsolete Blink popup blocker
    // removed javascript_can_open_windows_automatically preference.
    case Attribute::SpatialNavigationEnabled:
      preferences->spatial_navigation_enabled = enable;
      break;
    case Attribute::SupportsMultipleWindows:
      preferences->supports_multiple_windows = enable;
      break;
    case Attribute::CSSNavigationEnabled:
      preferences->css_navigation_enabled = enable;
      break;
    case Attribute::LocalStorageEnabled:
      preferences->local_storage_enabled = enable;
      break;
    case Attribute::WebSecurityEnabled:
      preferences->web_security_enabled = enable;
      if (!preferences->web_security_enabled) {
        GrantLoadLocalResources();
        SetCorsCorbDisabled(!preferences->web_security_enabled);
      }
      break;
    case Attribute::XFrameOptionsCrossOriginAllowed:
      preferences->x_frame_options_cross_origin_allowed = enable;
      break;
    case Attribute::KeepAliveWebApp:
      preferences->keep_alive_webapp = enable;
      break;
    case Attribute::RequestQuotaEnabled:
    case Attribute::DisallowScrollingInMainFrame:
    case Attribute::V8DateUseSystemLocaloffset:
    case Attribute::AdditionalFontFamilyEnabled:
      // TODO(jose.dapena): patches not ported
      NOTIMPLEMENTED() << "patches not ported";
      break;
    case Attribute::AllowThirdPartyCookies:
      if (enable) {
        web_preferences_->third_party_cookies_policy =
            blink::mojom::ThirdPartyCookiesPolicy::kAllow;
      } else {
        web_preferences_->third_party_cookies_policy =
            blink::mojom::ThirdPartyCookiesPolicy::kDeny;
      }
      break;
    default:
      break;
  }
}

void WebView::UpdatePreferencesAttribute(WebView::Attribute attribute,
                                         bool enable) {
  webview_preferences_list_[attribute] = enable;
  UpdatePreferencesAttributeForPrefs(web_preferences_.get(), attribute, enable);

  web_contents_->SetWebPreferences(*web_preferences_);
}

void WebView::SetNetworkQuietTimeout(double timeout) {
  TRACE_EVENT1("neva", "WebView::SetNetworkQuietTimeout", "timeout", timeout);
  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->SetNetworkQuietTimeout(timeout);
  }
}

void WebView::SetDisallowScrollbarsInMainFrame(bool disallow) {
  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->SetDisallowScrollbarsInMainFrame(disallow);
  }
}

void WebView::GrantLoadLocalResources() {
  if (auto* frame_host = web_contents_->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    if (client)
      client->GrantLoadLocalResources();
  }
}

void WebView::SetCorsCorbDisabled(bool disabled) {
  if (web_contents_->GetPrimaryMainFrame() &&
      web_contents_->GetPrimaryMainFrame()->GetProcess()) {
    GetAppRuntimeContentBrowserClient()->SetCorsCorbDisabled(
        web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(), disabled);
  }
}

void WebView::SetAllowLocalResourceLoad(bool enable) {
  TRACE_EVENT1("neva", "WebView::SetAllowLocalResourceLoad", "enable", enable);

  if (enable == allow_local_resources_load_)
    return;
  allow_local_resources_load_ = enable;
  if (allow_local_resources_load_)
    GrantLoadLocalResources();
}

bool WebView::GetAllowLocalResourceLoad() const {
  return allow_local_resources_load_;
}

void WebView::SetFontFamily(WebView::FontFamily font_family,
                            const std::string& font) {
  switch (font_family) {
    case FontFamily::StandardFont:
      web_preferences_->
          standard_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    case FontFamily::FixedFont:
      web_preferences_->
          fixed_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    case FontFamily::SerifFont:
      web_preferences_->
          serif_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    case FontFamily::SansSerifFont:
      web_preferences_->
          sans_serif_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    case FontFamily::CursiveFont:
      web_preferences_->
          cursive_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    case FontFamily::FantasyFont:
      web_preferences_->
          fantasy_font_family_map[blink::web_pref::kCommonScript] =
          base::ASCIIToUTF16(font.c_str());
      break;
    default:
      return;
  }

  web_contents_->SetWebPreferences(*web_preferences_);
}

void WebView::SetActiveOnNonBlankPaint(bool active) {
  TRACE_EVENT1("neva", "WebView::SetActiveOnNonBlankPaint", "active", active);

  active_on_non_blank_paint_ = active;
}

void WebView::SetViewportSize(int width, int height) {
  TRACE_EVENT2("neva", "WebView::SetViewportSize", "width", width, "height",
               height);

  if ((width == 0) || (height == 0))
    return;

  viewport_size_ = gfx::Size(width, height);
  UpdateViewportScaleFactor();
}

void WebView::NotifyMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  TRACE_EVENT1("neva", "WebView::NotifyMemoryPressure", "level", level);

  LOG(INFO) << "[MemoryPressure] " << __FUNCTION__ << " => Level: " << level;
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}

void WebView::SetVisible(bool visible) {
  TRACE_EVENT1("neva", "WebView::SetVisible", "visible", visible);

  if (visible)
    web_contents_->WasShown();
  else
    web_contents_->WasHidden();
}

void WebView::SetDatabaseIdentifier(const std::string& identifier) {
  NOTIMPLEMENTED();
  // TODO(jose.dapena): patch not ported
}

void WebView::SetVisibilityState(WebPageVisibilityState visibility_state) {
  mojom::VisibilityState app_runtime_visibility_state = mojom::VisibilityState::kNone;
  if (!ConvertVisibilityState(visibility_state, app_runtime_visibility_state))
    return;

  mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
  web_contents_->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&client);
  client->SetVisibilityState(app_runtime_visibility_state);
}

void WebView::DeleteWebStorages(const std::string& identifier) {
  TRACE_EVENT1("neva", "WebView::DeleteWebStorages", "identifier", identifier);

  content::BrowserContext* browser_context = profile_->GetBrowserContext();
  content::StoragePartition* storage_partition =
      browser_context->GetStoragePartition(nullptr);
  std::string origin = std::string("file://").append(identifier);
  storage_partition->GetDOMStorageContext()->DeleteLocalStorage(
      blink::StorageKey(url::Origin::Create(GURL(origin))), base::DoNothing());
}

void WebView::SetFocus(bool focus) {
  TRACE_EVENT1("neva", "WebView::SetFocus", "focus", focus);

  if (focus) {
    web_contents_->Focus();
  }

  content::RenderWidgetHost* const rwh =
      web_contents_->GetRenderViewHost()->GetWidget();

  if (rwh) {
    if (focus)
      rwh->Focus();
    else
      rwh->Blur();
  }
}

double WebView::GetZoomFactor() {
  return blink::PageZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(web_contents_.get()));
}

void WebView::SetZoomFactor(double factor) {
  TRACE_EVENT1("neva", "WebView::SetZoomFactor", "factor", factor);

  content::HostZoomMap::SetZoomLevel(web_contents_.get(),
                                     blink::PageZoomFactorToZoomLevel(factor));
}

void WebView::SetDoNotTrack(bool dnt) {
  TRACE_EVENT1("neva", "WebView::SetDoNotTrack", "dnt", dnt);

  blink::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  if (renderer_prefs->enable_do_not_track == dnt)
    return;

  renderer_prefs->enable_do_not_track = dnt;

  web_contents_->SyncRendererPrefs();
}

void WebView::ForwardAppRuntimeEvent(AppRuntimeEvent* event) {
  TRACE_EVENT0("neva", "WebView::ForwardAppRuntimeEvent");

  content::RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return;
  content::RenderWidgetHost* rwh = rwhv->GetRenderWidgetHost();
  if (!rwh)
    return;

  switch (event->GetType()) {
    case AppRuntimeEvent::MouseButtonRelease: {
      AppRuntimeMouseEvent* app_runtime_event =
          static_cast<AppRuntimeMouseEvent*>(event);
      ui::MouseEvent mouse_event = ui::MouseEvent(
          ui::ET_MOUSE_RELEASED,
          gfx::Point(app_runtime_event->GetX(), app_runtime_event->GetY()),
          gfx::Point(app_runtime_event->GetX(), app_runtime_event->GetY()),
          ui::EventTimeForNow(), app_runtime_event->GetFlags(), 0);

      blink::WebMouseEvent released_event = ui::MakeWebMouseEvent(mouse_event);

      rwh->ForwardMouseEvent(released_event);
      break;
    }
    case AppRuntimeEvent::MouseMove: {
      AppRuntimeMouseEvent* app_runtime_event =
          static_cast<AppRuntimeMouseEvent*>(event);
      ui::MouseEvent mouse_event = ui::MouseEvent(
          ui::ET_MOUSE_MOVED,
          gfx::Point(app_runtime_event->GetX(), app_runtime_event->GetY()),
          gfx::Point(app_runtime_event->GetX(), app_runtime_event->GetY()),
          ui::EventTimeForNow(), app_runtime_event->GetFlags(), 0);

      blink::WebMouseEvent moved_event = ui::MakeWebMouseEvent(mouse_event);

      rwh->ForwardMouseEvent(moved_event);
      break;
    }
    case AppRuntimeEvent::KeyPress:
    case AppRuntimeEvent::KeyRelease: {
      AppRuntimeKeyEvent* key_event = static_cast<AppRuntimeKeyEvent*>(event);
      int keycode = key_event->GetCode();

      content::NativeWebKeyboardEvent native_event(
          ui::KeyEvent(event->GetType() == AppRuntimeKeyEvent::KeyPress
                           ? ui::ET_KEY_PRESSED
                           : ui::ET_KEY_RELEASED,
                       ui::KeyboardCode(keycode), ui::DomCode::NONE,
                       key_event->GetFlags(), key_event->GetDomKey(),
                       base::TimeTicks()),
          wchar_t(keycode));

      native_event.windows_key_code = keycode;
      native_event.native_key_code = keycode;
      native_event.text[0] = 0;
      native_event.unmodified_text[0] = 0;
      native_event.SetType(event->GetType() == AppRuntimeKeyEvent::KeyPress
                               ? blink::WebInputEvent::Type::kKeyDown
                               : blink::WebInputEvent::Type::kKeyUp);
      rwh->ForwardKeyboardEvent(native_event);
      break;
    }
    default:
      break;
  }
}

bool WebView::CanGoBack() const {
  return web_contents_->GetController().CanGoBack();
}

void WebView::GoBack() {
  TRACE_EVENT0("neva", "WebView::GoBack");

  content::RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return;
  content::RenderWidgetHost* rwh = rwhv->GetRenderWidgetHost();
  if (!rwh)
    return;

  content::NativeWebKeyboardEvent native_event(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                   ui::DomCode::NONE, 0, ui::DomKey::GO_BACK,
                   base::TimeTicks()));

  rwh->ForwardKeyboardEvent(native_event);
}

void WebView::SendGetCookiesResponse(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  std::string cookie_line = net::CanonicalCookie::BuildCookieLine(cookie_list);
  if (webview_delegate_)
    webview_delegate_->SendCookiesForHostname(cookie_line);
}

void WebView::SetAdditionalContentsScale(float scale_x, float scale_y) {
  // NOTE(webOS): additional contents scale info is removed
}

void WebView::SetEnableHtmlSystemKeyboardAttr(bool enable) {
  TRACE_EVENT1("neva", "WebView::SetEnableHtmlSystemKeyboardAttr", "enable",
               enable);

  content::RenderWidgetHostViewAura* const host_view =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents_->GetRenderViewHost()->GetWidget()->GetView());

  if (!host_view)
    return;

  host_view->SetEnableHtmlSystemKeyboardAttr(enable);
}

void WebView::RequestInjectionLoading(const std::string& injection_name) {
  TRACE_EVENT1("neva", "WebView::RequestInjectionLoading", "injection_name",
               injection_name);

  injection_manager_->RequestLoadInjection(web_contents_->GetPrimaryMainFrame(),
                                           injection_name);
}

void WebView::RequestClearInjections() {
  TRACE_EVENT0("neva", "WebView::RequestClearInjections");

  injection_manager_->RequestUnloadInjections(web_contents_->GetPrimaryMainFrame());
}

void WebView::ResetStateToMarkNextPaint() {
  TRACE_EVENT0("neva", "WebView::ResetStateToMarkNextPaint");

  mojo::AssociatedRemote<mojom::AppRuntimeWebViewClient> client;
  web_contents_->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces()->
      GetInterface(&client);
  client->ResetStateToMarkNextPaint();
}

void WebView::DropAllPeerConnections(
    neva_app_runtime::DropPeerConnectionReason reason) {
  TRACE_EVENT0("neva", "WebView::DropAllPeerConnections");

  blink::mojom::DropPeerConnectionReason blink_mojom_reason;
  switch (reason) {
    case neva_app_runtime::DropPeerConnectionReason::
        kDropPeerConnectionReasonPageHidden:
      blink_mojom_reason = blink::mojom::DropPeerConnectionReason::kPageHidden;
      break;
    case neva_app_runtime::DropPeerConnectionReason::
        kDropPeerConnectionReasonUnknown:
    default:
      blink_mojom_reason = blink::mojom::DropPeerConnectionReason::kUnknown;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableDropAllPeerConnections))
    web_contents_->DropAllPeerConnections(blink_mojom_reason);
}

//////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsObserver implementation:

void WebView::RenderFrameCreated(content::RenderFrameHost* render_frame_host) {
  TRACE_EVENT0("neva", "WebView::RenderFrameCreated");

  SetSkipFrame(enable_skip_frame_);
  injection_manager_->RequestReloadInjections(render_frame_host);
}

void WebView::DidStartLoading() {
  TRACE_EVENT0("neva", "WebView::DidStartLoading");

  if (webview_delegate_)
    webview_delegate_->LoadStarted();
}

void WebView::DidStopLoading() {
  TRACE_EVENT0("neva", "WebView::DidStopLoading");

  if (webview_delegate_)
    webview_delegate_->LoadStopped();
}

void WebView::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                            const GURL& validated_url) {
  TRACE_EVENT1("neva", "WebView::DidFinishLoad", "validated_url",
               validated_url.spec());

#if defined(ENABLE_PLUGINS)
  if (!GetAppRuntimeContentBrowserClient()->PluginLoaded()) {
    GetAppRuntimeContentBrowserClient()->SetPluginLoaded(true);
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&GetPluginsCallback));
  }
#endif
  // Async notification is required for webOS WAM app exit logic which
  // depends on loading about:blank page
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebView::FinishLoadCallback, weak_factory_.GetWeakPtr(),
                     validated_url.spec()));
}

void WebView::FinishLoadCallback(const std::string& url) {
  if (webview_delegate_)
    webview_delegate_->LoadFinished(url);
}

void WebView::DidUpdateFaviconURL(
    content::RenderFrameHost* rfh,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  TRACE_EVENT0("neva", "WebView::DidUpdateFaviconURL");

  for (auto& candidate : candidates) {
    if (candidate->icon_type == blink::mojom::FaviconIconType::kFavicon &&
        !candidate->icon_url.is_empty()) {
      content::NavigationEntry* entry =
          web_contents()->GetController().GetActiveEntry();
      if (!entry)
        continue;
      content::FaviconStatus& favicon = entry->GetFavicon();
      favicon.url = candidate->icon_url;
      favicon.valid = favicon.url.is_valid();
      break;
    }
  }
}

void WebView::DidStartNavigation(content::NavigationHandle* navigation_handle) {
  TRACE_EVENT0("neva", "WebView::DidStartNavigation");

  if (!navigation_handle)
    return;

  if (webview_delegate_)
    webview_delegate_->DidStartNavigation(navigation_handle->GetURL().spec(),
                                          navigation_handle->IsInMainFrame());
}

void WebView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  TRACE_EVENT0("neva", "WebView::DidFinishNavigation");
  NEVA_DCHECK(navigation_handle);
  if (!navigation_handle)
    return;

  if (navigation_handle->GetNetErrorCode() != net::OK) {
    if (navigation_handle->IsInMainFrame())
      DispatchNetError(navigation_handle->GetURL(),
                       navigation_handle->GetNetErrorCode());
    if (navigation_handle->IsErrorPage())
      webview_delegate_->DidErrorPageLoadedFromNetErrorHelper();
    return;
  }

  if (navigation_handle->IsInMainFrame() && webview_delegate_) {
    webview_delegate_->NavigationHistoryChanged();
  }

  if (webview_delegate_ && navigation_handle->HasCommitted())
    webview_delegate_->DidFinishNavigation(navigation_handle->GetURL().spec(),
                                           navigation_handle->IsInMainFrame());

  UpdateViewportScaleFactor();
}

void WebView::DispatchNetError(const GURL& url, int error_code) {
  if (!webview_delegate_)
    return;
  std::string url_string = url.spec();
  if (error_code == net::ERR_ABORTED)
    webview_delegate_->LoadAborted(url_string);
  else
    webview_delegate_->LoadFailed(url_string, error_code, std::string());
}

void WebView::DidFailLoad(content::RenderFrameHost* render_frame_host,
                          const GURL& validated_url,
                          int error_code) {
  TRACE_EVENT2("neva", "WebView::DidFailLoad", "error_code", error_code,
               "validated_url", validated_url.spec());

  CHECK(render_frame_host);
  if (static_cast<content::RenderFrameHostImpl*>(render_frame_host)
          ->frame_tree_node()
          ->IsMainFrame()) {
    DispatchNetError(validated_url, error_code);
  }
}

void WebView::RenderProcessCreated(base::ProcessHandle handle) {
  TRACE_EVENT0("neva", "WebView::RenderProcessCreated");

  if (webview_delegate_)
    webview_delegate_->RenderProcessCreated(handle);
}

void WebView::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  TRACE_EVENT0("neva", "WebView::PrimaryMainFrameRenderProcessGone");

  if (webview_delegate_)
    webview_delegate_->RenderProcessGone();
}

void WebView::DOMContentLoaded(content::RenderFrameHost* render_frame_host) {
  TRACE_EVENT0("neva", "WebView::DOMContentLoaded");

  // TODO(pikulik): Should be revised!
  if (webview_delegate_ &&
      static_cast<content::RenderFrameHostImpl*>(render_frame_host)
          ->frame_tree_node()
          ->IsMainFrame())
    webview_delegate_->DocumentLoadFinished();

  for (const auto& css : injected_css_) {
    AddUserStyleSheetForFrame(css, render_frame_host);
  }
}

void WebView::DidDropAllPeerConnections(
    blink::mojom::DropPeerConnectionReason reason) {
  TRACE_EVENT0("neva", "WebView::DidDropAllPeerConnections");

  if (webview_delegate_) {
    neva_app_runtime::DropPeerConnectionReason app_runtime_reason;
    switch (reason) {
      case blink::mojom::DropPeerConnectionReason::kPageHidden:
        app_runtime_reason = neva_app_runtime::DropPeerConnectionReason::
            kDropPeerConnectionReasonPageHidden;
        break;
      case blink::mojom::DropPeerConnectionReason::kUnknown:
      default:
        app_runtime_reason = neva_app_runtime::DropPeerConnectionReason::
            kDropPeerConnectionReasonUnknown;
    }
    webview_delegate_->DidDropAllPeerConnections(app_runtime_reason);
  }
}

void WebView::DidCompleteSwap() {
  VLOG(3) << __func__;
  if (webview_delegate_)
    webview_delegate_->DidSwapCompositorFrame();
}

void WebView::DidFrameFocused() {
  TRACE_EVENT0("neva", "WebView::DidFrameFocused");

  if (webview_delegate_)
    webview_delegate_->DidFirstFrameFocused();
}

void WebView::UpdatePreferences() {
  TRACE_EVENT0("neva", "WebView::UpdatePreferences");

  web_contents_->SyncRendererPrefs();

  web_contents_->SetWebPreferences(*web_preferences_);
}

void WebView::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  TRACE_EVENT0("neva", "WebView::EnterFullscreenModeForTab");

  SwitchFullscreenModeForTab(web_contents_.get(), true);
}

void WebView::ExitFullscreenModeForTab(content::WebContents* web_contents) {
  TRACE_EVENT0("neva", "WebView::ExitFullscreenModeForTab");

  SwitchFullscreenModeForTab(web_contents, false);
}

bool WebView::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return full_screen_;
}

void WebView::NotifyRenderWidgetWasResized() {
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  if (!rvh)
    return;
  content::RenderWidgetHost* rwh = rvh->GetWidget();
  if (rwh)
    rwh->SynchronizeVisualProperties();
}

void WebView::UpdateViewportScaleFactor() {
  if (viewport_size_.IsEmpty())
    return;

  gfx::Size resolution = web_contents_->GetViewBounds().size();
  if (resolution.IsEmpty())
    return;

  const float width_scale = resolution.width() / float(viewport_size_.width());
  const float height_scale = resolution.height() / float(viewport_size_.height());
  const float scale = std::min(width_scale, height_scale);
  if (web_preferences_->default_minimum_page_scale_factor != scale) {
    web_preferences_->default_minimum_page_scale_factor = scale;
    UpdatePreferences();
  }
}

bool WebView::AudioCaptureAllowed() {
  if (webview_delegate_)
    return webview_delegate_->AcceptsAudioCapture();
  return false;
}

bool WebView::VideoCaptureAllowed() {
  if (webview_delegate_)
    return webview_delegate_->AcceptsVideoCapture();
  return false;
}

bool WebView::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  TRACE_EVENT0("neva", "WebView::CheckMediaAccessPermission");

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

void WebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  TRACE_EVENT0("neva", "WebView::RequestMediaAccessPermission");

  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback));
}

void WebView::SetSSLCertErrorPolicy(SSLCertErrorPolicy policy) {
  ssl_cert_error_policy_ = policy;
}

SSLCertErrorPolicy WebView::GetSSLCertErrorPolicy() const {
  return ssl_cert_error_policy_;
}

WebViewProfile* WebView::GetProfile() const {
  return profile_;
}

void WebView::SetProfile(WebViewProfile* profile) {
  TRACE_EVENT0("neva", "WebView::SetProfile");

  // FIXME: Possible memory leak. We need to destroy previous profile if
  // it's not default one. Default profile is shared between all webview.
  profile_ = profile;
}

void WebView::SwitchFullscreenModeForTab(content::WebContents* web_contents,
                                         bool enter_fullscreen) {
  full_screen_ = enter_fullscreen;
  NotifyRenderWidgetWasResized();
}

void WebView::OverrideWebkitPrefs(blink::web_pref::WebPreferences* prefs) {
  TRACE_EVENT0("neva", "WebView::OverrideWebkitPrefs");

  if (!web_preferences_)
    return;

  for (const auto& preference : webview_preferences_list_)
    UpdatePreferencesAttributeForPrefs(prefs, preference.first,
                                       preference.second);

  // Sync Fonts
  prefs->standard_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->standard_font_family_map[blink::web_pref::kCommonScript];
  prefs->fixed_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->fixed_font_family_map[blink::web_pref::kCommonScript];
  prefs->serif_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->serif_font_family_map[blink::web_pref::kCommonScript];
  prefs->sans_serif_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->sans_serif_font_family_map[blink::web_pref::kCommonScript];
  prefs->cursive_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->cursive_font_family_map[blink::web_pref::kCommonScript];
  prefs->fantasy_font_family_map[blink::web_pref::kCommonScript] =
      web_preferences_->fantasy_font_family_map[blink::web_pref::kCommonScript];

  // Sync scale factor
  prefs->default_minimum_page_scale_factor =
      web_preferences_->default_minimum_page_scale_factor;

  prefs->first_frame_policy = web_preferences_->first_frame_policy;

  // Sync cookies policy
  prefs->third_party_cookies_policy =
      web_preferences_->third_party_cookies_policy;
}

bool WebView::DecidePolicyForResponse(bool is_main_frame,
                                      int status_code,
                                      const std::string& url,
                                      const std::string& status_text) {
  TRACE_EVENT2("neva", "WebView::DecidePolicyForResponse", "is_main_frame",
               is_main_frame, "status_code", status_code);

  if (!webview_delegate_)
    return false;
  return webview_delegate_->DecidePolicyForResponse(is_main_frame, status_code,
                                                    url, status_text);
}

void WebView::SetV8SnapshotPath(const std::string& v8_snapshot_path) {
  TRACE_EVENT1("neva", "WebView::SetV8SnapshotPath", "v8_snapshot_path",
               v8_snapshot_path);

  GetAppRuntimeContentBrowserClient()->SetV8SnapshotPath(
      web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(), v8_snapshot_path);
}

void WebView::SetV8ExtraFlags(const std::string& v8_extra_flags) {
  TRACE_EVENT1("neva", "WebView::SetV8ExtraFlags", "v8_extra_flags",
               v8_extra_flags);

  GetAppRuntimeContentBrowserClient()->SetV8ExtraFlags(
      web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(), v8_extra_flags);
}

void WebView::SetUseNativeScroll(bool use_native_scroll) {
  GetAppRuntimeContentBrowserClient()->SetUseNativeScroll(
      web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(),
      use_native_scroll);
}

void WebView::SetFirstFramePolicy(FirstFramePolicy policy) {
  blink::FirstFramePolicy blink_policy;
  switch (policy) {
    case FirstFramePolicy::kImmediate:
      blink_policy = blink::FirstFramePolicy::kImmediate;
      break;
    case FirstFramePolicy::kContents:
      blink_policy = blink::FirstFramePolicy::kContents;
      break;
    default:
      blink_policy = blink::FirstFramePolicy::kContents;
      break;
  }

  if (web_preferences_->first_frame_policy == blink_policy)
    return;

  web_preferences_->first_frame_policy = blink_policy;
  web_contents_->SetWebPreferences(*web_preferences_);
}

WebView::FirstFramePolicy WebView::GetFirstFramePolicy() const {
  switch (web_preferences_->first_frame_policy) {
    case blink::FirstFramePolicy::kImmediate:
      return FirstFramePolicy::kImmediate;
    case blink::FirstFramePolicy::kContents:
    default:
      return FirstFramePolicy::kContents;
  }
}

void WebView::ActivateRendererCompositor() {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  if (!rvh)
    return;
  content::RenderWidgetHostImpl* rwhi =
      static_cast<content::RenderViewHostImpl*>(rvh)->GetWidget();
  if (rwhi)
    rwhi->ActivateRendererCompositor();
}

void WebView::DeactivateRendererCompositor() {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  if (!rvh)
    return;
  content::RenderWidgetHostImpl* rwhi =
      static_cast<content::RenderViewHostImpl*>(rvh)->GetWidget();
  if (rwhi)
    rwhi->DeactivateRendererCompositor();
}

}  // namespace neva_app_runtime
