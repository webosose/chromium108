// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"
#endif

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class NoopLock;
class WebAppDataRetriever;
class WebAppInstallFinalizer;

// Install web app from manifest for current `WebContents`.
class FetchManifestAndInstallCommand : public WebAppCommand {
 public:
  // `use_fallback` allows getting fallback information from current document
  // to enable installing a non-promotable site.
  FetchManifestAndInstallCommand(
      webapps::WebappInstallSource install_surface,
      base::WeakPtr<content::WebContents> contents,
      bool bypass_service_worker_check,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback,
      bool use_fallback,
      WebAppInstallFinalizer* install_finalizer,
      std::unique_ptr<WebAppDataRetriever> data_retriever);

  ~FetchManifestAndInstallCommand() override;

  Lock& lock() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  content::WebContents* GetInstallingWebContents() override;

  base::Value ToDebugValue() const override;

 private:
  void Abort(webapps::InstallResultCode code);
  bool IsWebContentsDestroyed();

  void FetchFallbackInstallInfo();
  void OnGetWebAppInstallInfo(
      std::unique_ptr<WebAppInstallInfo> fallback_web_app_info);
  void FetchManifest();
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    bool is_installable);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and an intent to the Play Store should be made, or
  // synchronously calls OnDidCheckForIntentToPlayStore() implicitly failing the
  // check if it cannot be made.
  void CheckForPlayStoreIntentOrGetIcons(base::flat_set<GURL> icon_urls,
                                         bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(base::flat_set<GURL> icon_urls,
                                      bool skip_page_favicons,
                                      const std::string& intent,
                                      bool should_intent_to_store);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns (Lacros adapter that calls
  // |OnDidCheckForIntentToPlayStore| based on |result|).
  void OnDidCheckForIntentToPlayStoreLacros(
      base::flat_set<GURL> icon_urls,
      bool skip_page_favicons,
      const std::string& intent,
      crosapi::mojom::IsInstallableResult result);
#endif

  void OnIconsRetrievedShowDialog(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnDialogCompleted(bool user_accepted,
                         std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnInstallFinalizedMaybeReparentTab(const AppId& app_id,
                                          webapps::InstallResultCode code,
                                          OsHooksErrors os_hooks_errors);

  void OnInstallCompleted(const AppId& app_id, webapps::InstallResultCode code);

  void LogInstallInfo();

  std::unique_ptr<NoopLock> noop_lock_;
  std::unique_ptr<AppLock> app_lock_;

  webapps::WebappInstallSource install_surface_;
  base::WeakPtr<content::WebContents> web_contents_;
  bool bypass_service_worker_check_;
  WebAppInstallDialogCallback dialog_callback_;
  OnceInstallCallback install_callback_;
  // Whether using fallback installation data from the document.
  bool use_fallback_ = false;

  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  InstallErrorLogEntry install_error_log_entry_;

  AppId app_id_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  blink::mojom::ManifestPtr opt_manifest_;
  base::Value::Dict debug_log_;

  base::WeakPtrFactory<FetchManifestAndInstallCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
