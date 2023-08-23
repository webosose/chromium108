// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_delegate.h"

#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/media_capture_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/value_builder.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"

#if defined(USE_NEVA_APPRUNTIME)
#include "content/public/browser/render_view_host.h"
#include "extensions/common/switches.h"
#include "neva/app_runtime/public/mojom/app_runtime_webview.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#endif  // defined(USE_NEVA_APPRUNTIME

#if defined(USE_NEVA_APPRUNTIME) && defined(OS_WEBOS)
#include "base/command_line.h"
#include "neva/app_runtime/browser/app_runtime_webview_controller_impl.h"
#include "neva/app_runtime/public/webview_controller_delegate.h"
#endif  // defined(USE_NEVA_APPRUNTIME) && defined(OS_WEBOS)

#if defined(USE_PLATFORM_LANGUAGE_LISTENER)
#include "extensions/shell/neva/platform_language_listener.h"
#endif

#if defined(USE_PLATFORM_APPLICATION_REGISTRATION)
#include "extensions/shell/neva/platform_register_app.h"
#endif

namespace extensions {

#if defined(USE_NEVA_APPRUNTIME) && defined(OS_WEBOS)
namespace {

const char kDevicePixelRatio[] = "devicePixelRatio";
const char kIdentifier[] = "identifier";
const char kInitialize[] = "initialize";

class ShellAppWebViewControllerDelegate
    : public neva_app_runtime::WebViewControllerDelegate {
 public:
  ShellAppWebViewControllerDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  void RunCommand(const std::string& name,
                  const std::vector<std::string>& arguments) override {}

  std::string RunFunction(const std::string& name,
                          const std::vector<std::string>&) override {
    if (name == kInitialize) {
      return extensions::DictionaryBuilder()
          .Set(kIdentifier, GetIdentifier())
          .ToJSON();
    } else if (name == kIdentifier) {
      return GetIdentifier();
    } else if (name == kDevicePixelRatio) {
      return GetDevicePixelRatio();
    }
    return std::string();
  }

 private:
  std::string GetIdentifier() {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    return cmd->GetSwitchValueASCII(extensions::switches::kWebOSAppId);
  }

  std::string GetDevicePixelRatio() {
    float device_scale_factor =
        web_contents_->GetRenderWidgetHostView()->GetDeviceScaleFactor();
    return std::to_string(device_scale_factor);
  }

  content::WebContents* web_contents_ = nullptr;
};

}  // namespace
#endif

ShellAppDelegate::ShellAppDelegate() {
}

ShellAppDelegate::~ShellAppDelegate() {
}

void ShellAppDelegate::InitWebContents(content::WebContents* web_contents) {
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
#if defined(USE_PLATFORM_LANGUAGE_LISTENER)
  content::WebContentsUserData<PlatformLanguageListener>::CreateForWebContents(
      web_contents);
#endif
#if defined(USE_PLATFORM_APPLICATION_REGISTRATION)
  content::WebContentsUserData<PlatformRegisterApp>::CreateForWebContents(
      web_contents);
#endif
#if defined(USE_NEVA_APPRUNTIME) && defined(OS_WEBOS)
  neva_app_runtime::AppRuntimeWebViewControllerImpl::CreateForWebContents(
      web_contents);
#endif
}

void ShellAppDelegate::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  // Only do this for the primary main frame.
  if (frame_host->IsInPrimaryMainFrame()) {
    // The views implementation of AppWindow takes focus via SetInitialFocus()
    // and views::WebView but app_shell is aura-only and must do it manually.
    content::WebContents* contents =
        content::WebContents::FromRenderFrameHost(frame_host);
    contents->Focus();

#if defined(USE_NEVA_APPRUNTIME)
    mojo::AssociatedRemote<neva_app_runtime::mojom::AppRuntimeWebViewClient>
        client;
    contents->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces()
        ->GetInterface(&client);

#if defined(ENABLE_MEMORYMANAGER_WEBAPI) && defined(OS_WEBOS)
    client->AddInjectionToLoad(std::string("v8/memorymanager"),
                               std::string("{}"));
#endif  // defined(ENABLE_MEMORYMANAGER_WEBAPI) && defined(OS_WEBOS)
#if defined(USE_NEVA_BROWSER_SERVICE)
    client->AddInjectionToLoad(std::string("v8/sitefilter"), std::string("{}"));
    client->AddInjectionToLoad(std::string("v8/popupblocker"),
                               std::string("{}"));
    client->AddInjectionToLoad(std::string("v8/cookiemanager"),
                               std::string("{}"));
    client->AddInjectionToLoad(std::string("v8/userpermission"),
                               std::string("{}"));
    client->AddInjectionToLoad(std::string("v8/mediacapture"),
                               std::string("{}"));
#endif

#if defined(OS_WEBOS)
    auto* webview_controller_impl =
        neva_app_runtime::AppRuntimeWebViewControllerImpl::FromWebContents(
            contents);

    // The method is called to notify of the creation of all RenderFrameHost
    // objects, including related to subframes with other WebContents, as in
    // the case of an iframe, for which no webview_controller_impl has been
    // created as content::WebContentsUserData. So webview_controller_impl
    // can be nullptr.
    if (!webview_controller_impl)
      return;

    shell_app_webview_controller_delegate_ =
        std::make_unique<ShellAppWebViewControllerDelegate>(contents);

    webview_controller_impl->SetDelegate(
        shell_app_webview_controller_delegate_.get());

    client->AddInjectionToLoad(std::string("v8/webosservicebridge"),
                               std::string("{}"));
#endif  // defined(OS_WEBOS)
#endif  // defined(USE_NEVA_APPRUNTIME)
  }
}

void ShellAppDelegate::ResizeWebContents(content::WebContents* web_contents,
                                         const gfx::Size& size) {
  NOTIMPLEMENTED();
}

content::WebContents* ShellAppDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellAppDelegate::AddNewContents(
    content::BrowserContext* context,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  NOTIMPLEMENTED();
}

void ShellAppDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  NOTIMPLEMENTED();
  listener->FileSelectionCanceled();
}

void ShellAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  media_capture_util::GrantMediaStreamRequest(web_contents, request,
                                              std::move(callback), extension);
}

bool ShellAppDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  media_capture_util::VerifyMediaAccessPermission(type, extension);
  return true;
}

int ShellAppDelegate::PreferredIconSize() const {
  return extension_misc::EXTENSION_ICON_SMALL;
}

void ShellAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  NOTIMPLEMENTED();
}

bool ShellAppDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return true;
}

void ShellAppDelegate::SetTerminatingCallback(base::OnceClosure callback) {
  // TODO(jamescook): Should app_shell continue to close the app window
  // manually or should it use a browser termination callback like Chrome?
}

bool ShellAppDelegate::TakeFocus(content::WebContents* web_contents,
                                 bool reverse) {
  return false;
}

content::PictureInPictureResult ShellAppDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  NOTREACHED();
  return content::PictureInPictureResult::kNotSupported;
}

void ShellAppDelegate::ExitPictureInPicture() {
  NOTREACHED();
}

}  // namespace extensions
