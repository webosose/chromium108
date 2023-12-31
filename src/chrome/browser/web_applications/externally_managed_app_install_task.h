// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}

namespace webapps {
enum class InstallResultCode;
enum class UninstallResultCode;
}

namespace web_app {

class WebAppUrlLoader;
class WebAppInstallFinalizer;
class WebAppCommandManager;
class WebAppUiManager;
class WebAppDataRetriever;

// Class to install WebApp from a WebContents. A queue of such tasks is owned by
// ExternallyManagedAppManager. Can only be called from the UI thread.
class ExternallyManagedAppInstallTask {
 public:
  using ResultCallback = base::OnceCallback<void(
      ExternallyManagedAppManager::InstallResult result)>;

  // Constructs a task that will install a Web App for |profile|.
  // |install_options| will be used to decide some of the properties of the
  // installed app e.g. open in a tab vs. window, installed by policy, etc.
  explicit ExternallyManagedAppInstallTask(
      Profile* profile,
      WebAppUrlLoader* url_loader,
      WebAppRegistrar* registrar,
      WebAppUiManager* ui_manager,
      WebAppInstallFinalizer* install_finalizer,
      WebAppCommandManager* command_manager,
      ExternalInstallOptions install_options);

  ExternallyManagedAppInstallTask(const ExternallyManagedAppInstallTask&) =
      delete;
  ExternallyManagedAppInstallTask& operator=(
      const ExternallyManagedAppInstallTask&) = delete;

  virtual ~ExternallyManagedAppInstallTask();

  // Temporarily takes a |load_url_result| to decide if a placeholder app should
  // be installed.
  // TODO(ortuno): Remove once loading is done inside the task.
  virtual void Install(content::WebContents* web_contents,
                       ResultCallback result_callback);

  const ExternalInstallOptions& install_options() { return install_options_; }

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

 private:
  // Install directly from a fully specified WebAppInstallInfo struct. Used
  // by system apps.
  void InstallFromInfo(ResultCallback result_callback);

  void OnWebContentsReady(content::WebContents* web_contents,
                          ResultCallback result_callback,
                          WebAppUrlLoader::Result prepare_for_load_result);
  void OnUrlLoaded(content::WebContents* web_contents,
                   ResultCallback result_callback,
                   WebAppUrlLoader::Result load_url_result);

  // result_callback could be called synchronously or asynchronously.
  void InstallPlaceholder(content::WebContents* web_contents,
                          ResultCallback result_callback);

  void FetchCustomIcon(const GURL& url,
                       content::WebContents* web_contents,
                       int retries_left,
                       ResultCallback callback);

  void OnCustomIconFetched(int retries_left,
                           content::WebContents* web_contents,
                           ResultCallback callback,
                           int id,
                           int http_status_code,
                           const GURL& image_url,
                           const std::vector<SkBitmap>& bitmaps,
                           const std::vector<gfx::Size>& sizes);

  void FinalizePlaceholderInstall(
      ResultCallback callback,
      absl::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
          bitmaps);

  void UninstallPlaceholderApp(content::WebContents* web_contents,
                               ResultCallback result_callback);
  void OnPlaceholderUninstalled(content::WebContents* web_contents,
                                ResultCallback result_callback,
                                webapps::UninstallResultCode code);
  void ContinueWebAppInstall(content::WebContents* web_contents,
                             ResultCallback result_callback);
  void OnWebAppInstalled(bool is_placeholder,
                         bool offline_install,
                         ResultCallback result_callback,
                         const AppId& app_id,
                         webapps::InstallResultCode code);
  void OnWebAppInstalledWithHooksErrors(bool is_placeholder,
                                        bool offline_install,
                                        ResultCallback result_callback,
                                        const AppId& app_id,
                                        webapps::InstallResultCode code,
                                        OsHooksErrors os_hooks_errors);
  void TryAppInfoFactoryOnFailure(
      ResultCallback result_callback,
      ExternallyManagedAppManager::InstallResult result);

  const raw_ptr<Profile> profile_;
  const raw_ptr<WebAppUrlLoader> url_loader_;
  const raw_ptr<WebAppRegistrar> registrar_;
  const raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  const raw_ptr<WebAppCommandManager> command_manager_;
  const raw_ptr<WebAppUiManager> ui_manager_;

  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  const ExternalInstallOptions install_options_;
  DataRetrieverFactory data_retriever_factory_;

  base::WeakPtrFactory<ExternallyManagedAppInstallTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_
