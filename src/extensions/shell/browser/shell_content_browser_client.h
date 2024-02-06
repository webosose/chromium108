// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if defined(USE_NEVA_APPRUNTIME)
#include "mojo/public/cpp/bindings/remote_set.h"
#include "neva/pal_service/public/proxy_setting_delegate.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#endif

class GURL;

#if defined(USE_NEVA_APPRUNTIME)
namespace pal {
class ProxySettingDelegate;
}  // namespace pal
#endif

namespace base {
class CommandLine;
}

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace content {
class BrowserContext;
#if defined(USE_NEVA_APPRUNTIME)
struct GlobalRequestID;
class LoginDelegate;
class WebContents;
#endif
}

namespace service_manager {
template <typename...>
class BinderRegistryWithArgs;
using BinderRegistry = BinderRegistryWithArgs<>;
}  // namespace service_manager

namespace extensions {
class Extension;
class ShellBrowserMainDelegate;
class ShellBrowserMainParts;

// Content module browser process support for app_shell.
class ShellContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ShellContentBrowserClient(
      ShellBrowserMainDelegate* browser_main_delegate);

  ShellContentBrowserClient(const ShellContentBrowserClient&) = delete;
  ShellContentBrowserClient& operator=(const ShellContentBrowserClient&) =
      delete;

  ~ShellContentBrowserClient() override;

  // Returns the single instance.
  static ShellContentBrowserClient* Get();

  // Returns the single browser context for app_shell.
  content::BrowserContext* GetBrowserContext();
  // Returns true if the given page is allowed to open a window of the given
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the UI thread.
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  // content::ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& site_url) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_process_id) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
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
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame_host,
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
#if defined(USE_NEVA_APPRUNTIME)
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params)
      override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_request_for_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  void SetProxyServer(const content::ProxySettings& proxy_settings) override;
  bool IsNevaDynamicProxyEnabled() override;
#endif
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
  void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  std::string GetUserAgent() override;
#if defined(USE_NEVA_APPRUNTIME)
  blink::UserAgentMetadata GetUserAgentMetadata() override;
#endif  // defined(USE_NEVA_APPRUNTIME)
#if defined(USE_NEVA_BROWSER_SERVICE)
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  void set_override_web_preferences_callback(
      base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
          callback) {
    override_web_preferences_callback_ = std::move(callback);
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
#endif

 protected:
  // Subclasses may wish to provide their own ShellBrowserMainParts.
  virtual std::unique_ptr<ShellBrowserMainParts> CreateShellBrowserMainParts(
      ShellBrowserMainDelegate* browser_main_delegate,
      bool is_integration_test);

 private:
  // Appends command line switches for a renderer process.
  void AppendRendererSwitches(base::CommandLine* command_line);

  // Returns the extension or app associated with |site_instance| or NULL.
  const Extension* GetExtension(content::SiteInstance* site_instance);

#if defined(USE_NEVA_APPRUNTIME)
  class StubBadgeService;

  void BindBadgeServiceForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver);

  std::unique_ptr<StubBadgeService> stub_badge_service_;

  // Store the path of V8 snapshot blob for app_shell.
  std::pair<int, std::string> v8_snapshot_path_;

  // Used when need run proxy service.
  scoped_refptr<pal::ProxySettingDelegate> proxy_setting_delegate_;
  mojo::RemoteSet<network::mojom::CustomProxyConfigClient>
      custom_proxy_config_clients_;
  net::AuthCredentials credentials_;
#endif

  // Owned by content::BrowserMainLoop.
  raw_ptr<ShellBrowserMainParts> browser_main_parts_;

  // Owned by ShellBrowserMainParts.
  raw_ptr<ShellBrowserMainDelegate> browser_main_delegate_;
#if defined(USE_NEVA_BROWSER_SERVICE)
  base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
      override_web_preferences_callback_;
#endif
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
