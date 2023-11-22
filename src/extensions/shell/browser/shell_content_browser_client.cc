// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_content_browser_client.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "components/nacl/common/buildflags.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "extensions/browser/api/messaging/messaging_api_message_filter.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/guest_view/extensions_guest_view.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/shell_browser_context.h"
#include "extensions/shell/browser/shell_browser_main_parts.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_navigation_ui_data.h"
#include "extensions/shell/browser/shell_speech_recognition_manager_delegate.h"
#include "extensions/shell/common/version.h"  // Generated file.
#include "neva/browser_service/browser/cookiemanager_service_impl.h"
#include "neva/browser_service/browser/popupblocker_service_impl.h"
#include "neva/browser_service/browser/userpermission_service_impl.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"  // nogncheck
#include "components/nacl/common/nacl_switches.h"      // nogncheck
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_data.h"
#endif

#if defined(USE_NEVA_APPRUNTIME)
#include "base/neva/base_switches.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/shell/common/shell_neva_switches.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "neva/app_runtime/browser/app_runtime_webview_controller_impl.h"
#include "neva/app_runtime/browser/app_runtime_webview_host_impl.h"
#include "neva/browser_service/public/mojom/constants.mojom.h"
#include "neva/user_agent/common/user_agent.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_neva_switches.h"
#endif
#if defined(USE_NEVA_BROWSER_SERVICE)
#include "neva/browser_service/browser/malware_url_loader_throttle.h"
#include "neva/browser_service/browser_service.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#endif

using base::CommandLine;
using content::BrowserContext;
#if defined(USE_NEVA_BROWSER_SERVICE)
using blink::web_pref::WebPreferences;
#endif
namespace extensions {
namespace {
#if defined(USE_NEVA_APPRUNTIME)
const char kCacheStoreFile[] = "Cache";
const char kCookieStoreFile[] = "Cookies";
const int kDefaultDiskCacheSize = 16 * 1024 * 1024;  // default size is 16MB
const char kWebexScheme[] = "webex";

void AuthRequestCallback(
    LoginAuthRequiredCallback callback,
    const absl::optional<net::AuthCredentials>& credentials,
    bool should_cancel) {
  if (credentials) {
    std::move(callback).Run(credentials);
    return;
  }
}
#endif

ShellContentBrowserClient* g_instance = nullptr;

}  // namespace

ShellContentBrowserClient::ShellContentBrowserClient(
    ShellBrowserMainDelegate* browser_main_delegate)
    : browser_main_parts_(nullptr),
      browser_main_delegate_(browser_main_delegate) {
  DCHECK(!g_instance);
  g_instance = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_instance = nullptr;
}

// static
ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_instance;
}

bool ShellContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
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
    bool* no_javascript_access) {
  *no_javascript_access = false;

#if defined(USE_NEVA_APPRUNTIME)
  if (browser::PopupBlockerServiceImpl::GetInstance()->IsBlocked(
          opener_top_level_frame_url, user_gesture, disposition)) {
    LOG(INFO) << __func__ << "Pop up window is blocked for this site: "
              << opener_url.spec().c_str();
    return false;
  }
#endif
  return true;
}

content::BrowserContext* ShellContentBrowserClient::GetBrowserContext() {
  return browser_main_parts_->browser_context();
}

std::unique_ptr<content::BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto browser_main_parts =
      CreateShellBrowserMainParts(browser_main_delegate_, is_integration_test);

  browser_main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

void ShellContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int render_process_id = host->GetID();
  BrowserContext* browser_context = browser_main_parts_->browser_context();
  host->AddFilter(
      new ExtensionMessageFilter(render_process_id, browser_context));
  host->AddFilter(
      new MessagingAPIMessageFilter(render_process_id, browser_context));
  // PluginInfoMessageFilter is not required because app_shell does not have
  // the concept of disabled plugins.
#if BUILDFLAG(ENABLE_NACL)
  host->AddFilter(new nacl::NaClHostMessageFilter(
      render_process_id, browser_context->IsOffTheRecord(),
      browser_context->GetPath()));
#endif
}

bool ShellContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  // This ensures that all render views created for a single app will use the
  // same render process (see content::SiteInstance::GetProcess). Otherwise the
  // default behavior of ContentBrowserClient will lead to separate render
  // processes for the background page and each app window view.
  return true;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  // Keep in sync with ProtocolHandlers added in
  // ShellBrowserContext::CreateRequestContext() and in
  // content::ShellURLRequestContextGetter::GetURLRequestContext().
  static const char* const kProtocolList[] = {
      url::kBlobScheme,
      content::kChromeDevToolsScheme,
      content::kChromeUIScheme,
      url::kDataScheme,
      url::kFileScheme,
      url::kFileSystemScheme,
      kExtensionScheme,
  };
  for (const char* scheme : kProtocolList) {
    if (url.SchemeIs(scheme))
      return true;
  }
  return false;
}

void ShellContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
#if defined(USE_NEVA_BROWSER_SERVICE)
  // We need to set the new cookie manager instance as it is changed
  // after a web page is opened because storage partition is changed
  network::mojom::CookieManager* cookie_manager =
      browser_main_parts_->browser_context()
          ->GetStoragePartition(site_instance, false)
          ->GetCookieManagerForBrowserProcess();
  browser::CookieManagerServiceImpl::Get()->SetNetworkCookieManager(
      cookie_manager);
#endif

  // If this isn't an extension renderer there's nothing to do.
  const Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

#if defined(USE_NEVA_APPRUNTIME)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kV8SnapshotBlobPath)) {
    v8_snapshot_path_ =
        std::make_pair(site_instance->GetProcess()->GetID(),
                       base::CommandLine::ForCurrentProcess()
                           ->GetSwitchValuePath(::switches::kV8SnapshotBlobPath)
                           .value());
  }
#endif
  ProcessMap::Get(browser_main_parts_->browser_context())
      ->Insert(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void ShellContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
  // Don't do anything if we're shutting down.
  if (content::BrowserMainRunner::ExitedMainMessageLoop())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  ProcessMap::Get(browser_main_parts_->browser_context())
      ->Remove(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  std::string process_type =
      command_line->GetSwitchValueASCII(::switches::kProcessType);
  if (process_type == ::switches::kRendererProcess)
    AppendRendererSwitches(command_line);
#if defined(USE_NEVA_APPRUNTIME)
  // Append v8 snapshot path if given
  if (v8_snapshot_path_.first == child_process_id) {
    command_line->AppendSwitchPath(::switches::kV8SnapshotBlobPath,
                                   base::FilePath(v8_snapshot_path_.second));
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kUseOzoneWaylandVkb))
    command_line->AppendSwitch(::switches::kUseOzoneWaylandVkb);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kOzoneWaylandUseXDGShell))
    command_line->AppendSwitch(::switches::kOzoneWaylandUseXDGShell);
#endif
#if defined(OS_WEBOS)
  command_line->AppendSwitch(::switches::kDisableQuic);
#endif
}

content::SpeechRecognitionManagerDelegate*
ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new speech::ShellSpeechRecognitionManagerDelegate();
}

content::BrowserPpapiHost*
ShellContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
#if BUILDFLAG(ENABLE_NACL)
  content::BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_LOADER);
  while (!iter.Done()) {
    nacl::NaClProcessHost* host = static_cast<nacl::NaClProcessHost*>(
        iter.GetDelegate());
    if (host->process() &&
        host->process()->GetData().id == plugin_process_id) {
      // Found the plugin.
      return host->browser_ppapi_host();
    }
    ++iter;
  }
#endif
  return nullptr;
}

void ShellContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(kExtensionScheme);
}

std::unique_ptr<content::DevToolsManagerDelegate>
ShellContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<content::ShellDevToolsManagerDelegate>(
      GetBrowserContext());
}

void ShellContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  associated_registry->AddInterface<mojom::EventRouter>(base::BindRepeating(
      &EventRouter::BindForRenderer, render_process_host->GetID()));
  associated_registry->AddInterface<guest_view::mojom::GuestViewHost>(
      base::BindRepeating(&ExtensionsGuestView::CreateForComponents,
                          render_process_host->GetID()));
  associated_registry->AddInterface<extensions::mojom::GuestView>(
      base::BindRepeating(&ExtensionsGuestView::CreateForExtensions,
                          render_process_host->GetID()));
#if defined(USE_NEVA_BROWSER_SERVICE)
  auto sitefilter_service =
      [](mojo::PendingReceiver<browser::mojom::SiteFilterService> receiver) {
        browser::BrowserService::GetBrowserService()->BindSiteFilterService(
            std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(sitefilter_service),
                         content::GetUIThreadTaskRunner({}));
  auto popupblocker_service =
      [](mojo::PendingReceiver<browser::mojom::PopupBlockerService> receiver) {
        browser::BrowserService::GetBrowserService()->BindPopupBlockerService(
            std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(popupblocker_service),
                         content::GetUIThreadTaskRunner({}));
  auto cookiemanager_service =
      [](mojo::PendingReceiver<browser::mojom::CookieManagerService> receiver) {
        browser::BrowserService::GetBrowserService()->BindCookieManagerService(
            std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(cookiemanager_service),
                         content::GetUIThreadTaskRunner({}));
  auto userpermission_service =
      [](mojo::PendingReceiver<browser::mojom::UserPermissionService>
             receiver) {
        browser::BrowserService::GetBrowserService()->BindUserPermissionService(
            std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(userpermission_service),
                         content::GetUIThreadTaskRunner({}));
  auto mediacapture_service =
      [](mojo::PendingReceiver<browser::mojom::MediaCaptureService> receiver) {
        browser::BrowserService::GetBrowserService()->BindMediaCaptureService(
            std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(mediacapture_service),
                         content::GetUIThreadTaskRunner({}));
  auto customuseragent_service =
      [](mojo::PendingReceiver<browser::mojom::CustomUserAgentService>
             receiver) {
        browser::BrowserService::GetBrowserService()
            ->BindCustomUserAgentService(std::move(receiver));
      };
  registry->AddInterface(base::BindRepeating(customuseragent_service),
                         content::GetUIThreadTaskRunner({}));
#endif
}

void ShellContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<extensions::mojom::LocalFrameHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<extensions::mojom::LocalFrameHost>
                 receiver) {
            ExtensionWebContentsObserver::BindLocalFrameHost(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#if defined(USE_NEVA_APPRUNTIME)
  associated_registry.AddInterface<blink::mojom::AppRuntimeBlinkDelegate>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 blink::mojom::AppRuntimeBlinkDelegate> receiver) {
            neva_app_runtime::AppRuntimeWebViewHostImpl::
                BindAppRuntimeBlinkDelegate(std::move(receiver),
                                            render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<
      neva_app_runtime::mojom::AppRuntimeWebViewHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             neva_app_runtime::mojom::AppRuntimeWebViewHost> receiver) {
        neva_app_runtime::AppRuntimeWebViewHostImpl::BindAppRuntimeWebViewHost(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<
      neva_app_runtime::mojom::AppRuntimeWebViewController>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             neva_app_runtime::mojom::AppRuntimeWebViewController> receiver) {
        neva_app_runtime::AppRuntimeWebViewControllerImpl::
            BindAppRuntimeWebViewController(std::move(receiver),
                                            render_frame_host);
      },
      &render_frame_host));
#endif  // USE_NEVA_APPRUNTIME
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ShellContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  throttles.push_back(
      std::make_unique<ExtensionNavigationThrottle>(navigation_handle));
  return throttles;
}

std::unique_ptr<content::NavigationUIData>
ShellContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  return std::make_unique<ShellNavigationUIData>(navigation_handle);
}

void ShellContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(factories);

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(), ukm_source_id,
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
}

void ShellContentBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionWorkerMainResourceURLLoaderFactory(
          browser_context));
}

void ShellContentBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
}

void ShellContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const absl::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(factories);

  factories->emplace(extensions::kExtensionScheme,
                     extensions::CreateExtensionURLLoaderFactory(
                         render_process_id, render_frame_id));
}

bool ShellContentBrowserClient::WillCreateURLLoaderFactory(
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
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  bool use_proxy = web_request_api->MaybeProxyURLLoaderFactory(
      browser_context, frame, render_process_id, type, std::move(navigation_id),
      ukm_source_id, factory_receiver, header_client);
  if (bypass_redirect_checks)
    *bypass_redirect_checks = use_proxy;
  return use_proxy;
}

bool ShellContentBrowserClient::HandleExternalProtocol(
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
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
#if defined(USE_NEVA_APPRUNTIME)
  if (url.scheme() == kWebexScheme)
    return true;
#endif
  return false;
}

void ShellContentBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
}

base::FilePath
ShellContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return GetBrowserContext()->GetPath();
}

std::string ShellContentBrowserClient::GetUserAgent() {
#if defined(USE_NEVA_APPRUNTIME)
  return neva_user_agent::GetDefaultUserAgent();
#else
  // Must contain a user agent string for version sniffing. For example,
  // pluginless WebRTC Hangouts checks the Chrome version number.
  return content::BuildUserAgentFromProduct("Chrome/" PRODUCT_VERSION);
#endif  // defined(USE_NEVA_APPRUNTIME)
}

#if defined(USE_NEVA_APPRUNTIME)
blink::UserAgentMetadata ShellContentBrowserClient::GetUserAgentMetadata() {
  return neva_user_agent::GetDefaultUserAgentMetadata();
}
#endif  // defined(USE_NEVA_APPRUNTIME)

std::unique_ptr<ShellBrowserMainParts>
ShellContentBrowserClient::CreateShellBrowserMainParts(
    ShellBrowserMainDelegate* browser_main_delegate,
    bool is_integration_test) {
  return std::make_unique<ShellBrowserMainParts>(browser_main_delegate,
                                                 is_integration_test);
}

void ShellContentBrowserClient::AppendRendererSwitches(
    base::CommandLine* command_line) {
  static const char* const kSwitchNames[] = {
      switches::kAllowlistedExtensionID,
      switches::kDEPRECATED_AllowlistedExtensionID,
      // TODO(jamescook): Should we check here if the process is in the
      // extension service process map, or can we assume all renderers are
      // extension renderers?
      switches::kExtensionProcess,
  };
  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchNames, std::size(kSwitchNames));

#if BUILDFLAG(ENABLE_NACL)
  static const char* const kNaclSwitchNames[] = {
      ::switches::kEnableNaClDebug,
  };
  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kNaclSwitchNames, std::size(kNaclSwitchNames));
#endif  // BUILDFLAG(ENABLE_NACL)
}

const Extension* ShellContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(site_instance->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}

#if defined(USE_NEVA_APPRUNTIME)
std::unique_ptr<content::LoginDelegate>
ShellContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  BrowserContext* browser_context = GetBrowserContext();
  extensions::WebRequestAPI* api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  auto continuation =
      base::BindOnce(&AuthRequestCallback, std::move(auth_required_callback));

  if (api->MaybeProxyAuthRequest(
          browser_context, auth_info, std::move(response_headers), request_id,
          is_request_for_main_frame, std::move(continuation))) {
  }

  return std::make_unique<content::LoginDelegate>();
}

content::StoragePartitionConfig
ShellContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  content::StoragePartitionConfig storage_partition_config =
      content::StoragePartitionConfig::CreateDefault(browser_context);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (site.SchemeIs(extensions::kExtensionScheme)) {
    // The host in an extension site URL is the extension_id.
    CHECK(site.has_host());
    return extensions::util::GetStoragePartitionConfigForExtensionId(
        site.host(), browser_context);
  }
#endif

  return storage_partition_config;
}

void ShellContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
#if defined(OS_WEBOS)
  network_service->DisableQuic();
#endif
}

void ShellContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language = "en-us,en";
  network_context_params->file_paths =
      ::network::mojom::NetworkContextFilePaths::New();
  network_context_params->file_paths->data_directory = context->GetPath();
  network_context_params->file_paths->cookie_database_name =
      base::FilePath(kCookieStoreFile);
  network_context_params->enable_encrypted_cookies = false;

  int disk_cache_size = kDefaultDiskCacheSize;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(::switches::kShellDiskCacheSize)) {
    if (!base::StringToInt(
            cmd_line->GetSwitchValueASCII(::switches::kShellDiskCacheSize),
            &disk_cache_size) ||
        disk_cache_size < 0) {
      LOG(ERROR) << __func__ << " invalid value("
                 << cmd_line->GetSwitchValueASCII(
                        ::switches::kShellDiskCacheSize)
                 << ") for the command-line switch of --"
                 << ::switches::kShellDiskCacheSize;
      disk_cache_size = kDefaultDiskCacheSize;
    }
  }

  network_context_params->http_cache_max_size = disk_cache_size;
  network_context_params->http_cache_directory =
      context->GetPath().Append(kCacheStoreFile);
}

// Implements a stub BadgeService. This implementation does nothing, but is
// required because inbound Mojo messages which do not have a registered
// handler are considered an error, and the render process is terminated.
// See https://crbug.com/1090429
class ShellContentBrowserClient::StubBadgeService
    : public blink::mojom::BadgeService {
 public:
  StubBadgeService() = default;
  StubBadgeService(const StubBadgeService&) = delete;
  StubBadgeService& operator=(const StubBadgeService&) = delete;
  ~StubBadgeService() override = default;

  void Bind(mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // blink::mojom::BadgeService:
  void SetBadge(blink::mojom::BadgeValuePtr value) override {}
  void ClearBadge() override {}

 private:
  mojo::ReceiverSet<blink::mojom::BadgeService> receivers_;
};

void ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&ShellContentBrowserClient::BindBadgeServiceForFrame,
                          base::Unretained(this)));
}

void ShellContentBrowserClient::BindBadgeServiceForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  if (!stub_badge_service_)
    stub_badge_service_ = std::make_unique<StubBadgeService>();

  stub_badge_service_->Bind(std::move(receiver));
}
#endif  // defined(USE_NEVA_APPRUNTIME)

#if defined(USE_NEVA_BROWSER_SERVICE)
void ShellContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    WebPreferences* prefs) {
  prefs->cookie_enabled =
      browser::CookieManagerServiceImpl::Get()->IsCookieEnabled();
  if (override_web_preferences_callback_)
    override_web_preferences_callback_.Run(prefs);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ShellContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;
  if (browser_main_parts_->malware_detection_service()) {
    result.push_back(std::make_unique<neva::MalwareURLLoaderThrottle>(
        browser_main_parts_->malware_detection_service()));
  }
  return result;
}

scoped_refptr<network::SharedURLLoaderFactory>
ShellContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  return GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}
#endif

}  // namespace extensions
