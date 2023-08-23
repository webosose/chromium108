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

#ifndef NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "neva/app_runtime/browser/app_runtime_browser_main_parts.h"
#include "neva/app_runtime/browser/net/app_runtime_proxying_url_loader_factory.h"
#include "neva/app_runtime/browser/net/app_runtime_web_request_handler.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"

namespace content {
class LoginDelegate;
class NavigationUIData;
class RenderFrameHost;
class PlatformNotificationService;
struct GlobalRequestID;
}  // namespace content

namespace pal {
class NotificationManagerDelegate;
}

namespace neva_app_runtime {

class AppRuntimeBrowserMainExtraParts;
class AppRuntimeQuotaPermissionDelegate;
struct ProxySettings;

class AppRuntimeContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit AppRuntimeContentBrowserClient(
      AppRuntimeQuotaPermissionDelegate* quota_permission_delegate);
  ~AppRuntimeContentBrowserClient() override;

  void SetBrowserExtraParts(
      AppRuntimeBrowserMainExtraParts* browser_extra_parts);

  // content::ContentBrowserClient implementations
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;

  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback) override;

  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;

  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;

  bool ShouldEnableStrictSiteIsolation() override;
  bool ShouldIsolateErrorPage(bool is_main_frame) override;

  bool IsFileAccessAllowedFromNetwork() const override;
  bool IsFileSchemeNavigationAllowed(const GURL& url,
                                     int render_frame_id,
                                     bool browser_initiated) override;

  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;

  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;

  bool HasQuotaSettings() const override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) const override;

  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;

#if defined(USE_NEVA_CHROME_EXTENSIONS)
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;

  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;

  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;

  void OnWebContentsCreated(content::WebContents* web_contents) override;

  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;

  void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;

  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;

  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      ukm::SourceIdObj ukm_source_id,
      NonNetworkURLLoaderFactoryMap* factories) override;

  void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;

  void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;

  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const absl::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;

  bool ShouldSendOutermostOriginToRenderer(
      const url::Origin& outermost_origin) override;
#endif  // defined(USE_NEVA_CHROME_EXTENSIONS)

  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      absl::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;

  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) override;

  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;

  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      int frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const absl::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;

  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::BrowserContext* resource_context) override;

  std::string GetUserAgent() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;

  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;

  AppRuntimeBrowserMainParts* GetMainParts() { return main_parts_; }

  void SetProxyServer(const ProxySettings& proxy_settings);

#if defined(ENABLE_PLUGINS)
  bool PluginLoaded() const { return plugin_loaded_; }
  void SetPluginLoaded(bool loaded) { plugin_loaded_ = loaded; }
#endif

  void SetV8SnapshotPath(int child_process_id, const std::string& path);
  void SetV8ExtraFlags(int child_process_id, const std::string& flags);
  void SetUseNativeScroll(int child_process_id, bool use_native_scroll);

  void AppendExtraWebSocketHeader(const std::string& key,
                                  const std::string& value);

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;

  void SetCorsCorbDisabled(int process_id, bool disabled);
  void SetCorsCorbDisabledForURL(const GURL& url, bool disabled);

 private:
  class StubBadgeService;

  void BindBadgeServiceForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  std::unique_ptr<StubBadgeService> stub_badge_service_;

  AppRuntimeBrowserMainExtraParts* browser_extra_parts_ = nullptr;
  AppRuntimeBrowserMainParts* main_parts_ = nullptr;

  AppRuntimeQuotaPermissionDelegate* quota_permission_delegate_ = nullptr;
  mojo::Remote<network::mojom::CustomProxyConfigClient>
      custom_proxy_config_client_;
  mojo::Remote<network::mojom::ExtraHeaderNetworkDelegate> network_delegate_;

#if defined(ENABLE_PLUGINS)
  bool plugin_loaded_ = false;
#endif

  std::map<int, std::string> v8_snapshot_pathes_;
  std::map<int, std::string> v8_extra_flags_;
  net::AuthCredentials credentials_;

  std::unique_ptr<pal::NotificationManagerDelegate>
      notification_manager_delegate_;

  uint64_t url_factory_next_id_ = 0;

  // Stores (int child_process_id, bool use_native_scroll) and apply the flags
  // related to native scroll when use_native_scroll flag for the render process
  // is true.
  std::map<int, bool> use_native_scroll_map_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_CONTENT_BROWSER_CLIENT_H_
