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

#ifndef NEVA_APP_RUNTIME_APP_APP_RUNTIME_PAGE_CONTENTS_H_
#define NEVA_APP_RUNTIME_APP_APP_RUNTIME_PAGE_CONTENTS_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/auth.h"
#include "net/base/network_anonymization_key.h"
#include "neva/app_runtime/app/app_runtime_js_dialog_manager_delegate.h"
#include "neva/app_runtime/app/app_runtime_page_contents_delegate.h"
#include "neva/app_runtime/browser/app_runtime_web_contents_delegate.h"
#include "neva/app_runtime/public/app_runtime_constants.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace content {
class WebContents;
struct MediaStreamRequest;
}  // namespace content

namespace net {
class AuthChallengeInfo;
class NetworkAnonymizationKey;
}  // namespace net

namespace neva_app_runtime {

class JSDialogManager;
class PageView;
class VisibleRegionCapture;
class WebAppInjectionManager;

class PageContents : public AppRuntimeWebContentsDelegate,
                     public content::WebContentsObserver,
                     public JSDialogManagerDelegate {
 public:
  struct CreateParams {
    CreateParams();
    CreateParams(const CreateParams&);
    CreateParams& operator=(const CreateParams&);
    ~CreateParams();

    int width = 0;
    int height = 0;
    PageContentsDelegate* delegate = nullptr;
    std::list<std::string> injections;
    bool inspectable = false;
    bool allow_file_access_from_file_urls = false;
    bool allow_universal_access_from_file_urls = false;
    std::string app_id;
    std::string storage_partition_name;
    bool storage_partition_off_the_record = false;
    std::string user_agent;
    bool active = false;
    bool error_page_hiding = false;
    absl::optional<bool> default_access_to_media = absl::nullopt;
  };

  explicit PageContents(const CreateParams& params);
  PageContents(const PageContents&) = delete;
  PageContents& operator=(const PageContents&) = delete;
  ~PageContents() override;

  void SetDelegate(PageContentsDelegate* delegate);
  PageContentsDelegate* GetDelegate() const;

  uint64_t GetID() const;
  content::WebContents* GetWebContents() const;
  void Activate();
  void CaptureVisibleRegion(const std::string& format, int quality = 90);
  void AckPermission(bool ack, uint64_t id);
  void AckAuthChallenge(const std::string& login,
                        const std::string& passwd,
                        const std::string& url);
  void ClearData(const std::string& clear_options,
                 const std::string& clear_data_type_set);
  void Deactivate();
  void ExecuteJavaScriptInAllFrames(const std::string& code_string);
  void ExecuteJavaScriptInMainFrame(const std::string& code_string);
  bool CanGoBack() const;
  bool CanGoForward() const;
  void CloseJSDialog(bool success, const std::string& response);

  std::string GetAcceptedLanguages() const;
  bool GetErrorPageHiding() const;
  std::string GetUserAgent() const;
  double GetZoomFactor() const;
  void GoBack();
  void GoForward();
  bool IsActive();
  bool LoadURL(std::string url_string);
  bool Reload();
  void ReloadNoWarranty();
  void ResumeDOM();
  void ResumeMedia();
  void SetAcceptedLanguages(std::string languages);
  void SetErrorPageHiding(bool enable);
  void SetFocus();
  // color is a RGBA or RGB string like #FFFFFFFF, #FFFFFF, #FFFF or #FFF
  void SetPageBaseBackgroundColor(std::string color);
  void SetZoomFactor(double factor);
  void Stop();
  void SetUserAgentOverride(const std::string& user_agent);
  void SuspendDOM();
  void SuspendMedia();
  void UpdatePreferredLanguage(std::string language);

  PageView* GetParentPageView() const;

  // WebContentsObserver
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void LoadProgressChanged(double progress) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void OnRendererUnresponsive(
      content::RenderProcessHost* render_process_host) override;
  void OnRendererResponsive(
      content::RenderProcessHost* render_process_host) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // AppRuntimeWebContentsDelegate
  void SetSSLCertErrorPolicy(SSLCertErrorPolicy policy) override;
  SSLCertErrorPolicy GetSSLCertErrorPolicy() const override;

  // WebContentsDelegate
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void FullscreenStateChangedForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents*) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

  // JSDialogManagerDelegate
  bool RunJSDialog(const std::string& type,
                   const std::string& message) override;
 private:
  friend PageView;
  static std::unique_ptr<content::WebContents> CreateWebContents(
      const PageContents::CreateParams& params);
  static std::unique_ptr<content::WebContents> ReCreateWebContents(
      content::BrowserContext* browser_context,
      const content::SessionStorageNamespaceMap& session_storage_namespace);

  PageContents(std::unique_ptr<content::WebContents> new_contents,
               const CreateParams& params);

  void SetParentPageView(PageView* page_view);
  void SetRendererPreferences(const CreateParams& params);
  void SetWebPreferences(const CreateParams& params);
  void RequestAllInjectionsLoading();
  void OnCaptureVisibleRegion(std::string base64);
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  struct MediaAccessPermissionInfo {
    MediaAccessPermissionInfo();
    MediaAccessPermissionInfo(
        const blink::mojom::StreamDevicesSet& stream_devices_set,
        content::MediaResponseCallback callback);
    MediaAccessPermissionInfo(MediaAccessPermissionInfo&&);
    ~MediaAccessPermissionInfo();

    static uint64_t id;
    blink::mojom::StreamDevicesSetPtr stream_devices_set_;
    content::MediaResponseCallback callback;
  };
  std::map<uint64_t, MediaAccessPermissionInfo> media_access_requests_;

  const uint64_t id_ = 0;
  PageView* parent_page_view_ = nullptr;
  PageContentsDelegate* delegate_ = nullptr;
  PageContentsDelegate stub_delegate_;
  bool page_requested_fullscreen_ = false;
  std::unique_ptr<content::WebContents> web_contents_;
  absl::optional<bool> default_access_to_media_ = absl::nullopt;
  std::unique_ptr<JSDialogManager> js_dialog_manager_;
  std::list<std::string> injections_;
  std::unique_ptr<WebAppInjectionManager> injection_manager_;
  std::string user_agent_;
  SSLCertErrorPolicy ssl_cert_error_policy_ = SSL_CERT_ERROR_POLICY_DEFAULT;
  // zoom changing subscription
  raw_ptr<content::HostZoomMap> host_zoom_map_ = nullptr;
  base::CallbackListSubscription zoom_changed_subscription_;
  // data for backup session
  content::SessionStorageNamespaceMap session_storage_namespace_map_;
  content::BrowserContext* browser_context_ = nullptr;
  std::string last_commited_url_;
  bool error_page_hiding_;
  std::unique_ptr<VisibleRegionCapture> visible_region_capture_;
  // login request data
  absl::optional<net::AuthChallengeInfo> auth_challenge_;
  net::NetworkAnonymizationKey network_anonymization_key_;
  base::WeakPtrFactory<PageContents> weak_ptr_factory_{this};
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_APP_APP_RUNTIME_PAGE_CONTENTS_H_
