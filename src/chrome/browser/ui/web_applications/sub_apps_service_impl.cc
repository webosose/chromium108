// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/sub_app_install_command.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using blink::mojom::SubAppsServiceAddResult;
using blink::mojom::SubAppsServiceAddResultCode;
using blink::mojom::SubAppsServiceAddResultPtr;
using blink::mojom::SubAppsServiceListResult;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

std::vector<blink::mojom::SubAppsServiceAddResultPtr> ResultsToMojo(
    std::vector<
        std::pair<UnhashedAppId, blink::mojom::SubAppsServiceAddResultCode>>
        sub_apps_idl) {
  std::vector<blink::mojom::SubAppsServiceAddResultPtr> subapps;
  for (const auto& [app_id, install_result_code] : sub_apps_idl) {
    blink::mojom::SubAppsServiceAddResultPtr mojom_pair =
        blink::mojom::SubAppsServiceAddResult::New();
    mojom_pair->unhashed_app_id = app_id;
    mojom_pair->result_code = install_result_code;
    subapps.push_back(std::move(mojom_pair));
  }
  return subapps;
}

std::vector<std::pair<UnhashedAppId, GURL>> InstallParamsFromMojo(
    std::vector<blink::mojom::SubAppsServiceAddInfoPtr> sub_apps_mojo) {
  std::vector<std::pair<UnhashedAppId, GURL>> subapps;
  for (const auto& pair : sub_apps_mojo) {
    subapps.emplace_back(UnhashedAppId(pair->unhashed_app_id),
                         pair->install_url);
  }
  return subapps;
}

WebAppProvider* GetWebAppProvider(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  auto* provider = WebAppProvider::GetForWebContents(initiator_web_contents);
  DCHECK(provider);
  return provider;
}

const AppId* GetAppId(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  return WebAppTabHelper::GetAppId(initiator_web_contents);
}

void OnAdd(
    SubAppsServiceImpl::AddCallback result_callback,
    std::vector<std::pair<UnhashedAppId,
                          blink::mojom::SubAppsServiceAddResultCode>> results) {
  std::move(result_callback).Run(ResultsToMojo(std::move(results)));
}

void OnRemove(SubAppsServiceImpl::RemoveCallback result_callback,
              webapps::UninstallResultCode code) {
  std::move(result_callback)
      .Run(code == webapps::UninstallResultCode::kSuccess
               ? SubAppsServiceResult::kSuccess
               : SubAppsServiceResult::kFailure);
}

}  // namespace

SubAppsServiceImpl::SubAppsServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsServiceImpl::~SubAppsServiceImpl() = default;

// static
void SubAppsServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsService> receiver) {
  CHECK(render_frame_host);

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    return;
  }

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new SubAppsServiceImpl(*render_frame_host, std::move(receiver));
}

void SubAppsServiceImpl::Add(
    std::vector<blink::mojom::SubAppsServiceAddInfoPtr> sub_apps,
    AddCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Add, weak_ptr_factory_.GetWeakPtr(),
                       std::move(sub_apps), std::move(result_callback)));
    return;
  }

  const AppId* parent_app_id = GetAppId(render_frame_host());
  // Verify that the calling app is installed itself. This check is done here
  // and not in `CreateIfAllowed` because of a potential race between doing the
  // check there and then running the current function, and the parent app being
  // installed/uninstalled.
  if (!parent_app_id) {
    std::vector<SubAppsServiceAddResultPtr> result;
    for (const auto& sub_app : sub_apps) {
      result.emplace_back(SubAppsServiceAddResult::New(
          sub_app->unhashed_app_id,
          SubAppsServiceAddResultCode::kParentAppUninstalled));
    }
    return std::move(result_callback).Run(/*mojom_results=*/std::move(result));
  }

  const GURL& parent_app_url = render_frame_host().GetLastCommittedURL();

  // Check that each sub app's install url has the same origin as the parent
  // app and that the unhashed app id is a valid URL.
  for (const blink::mojom::SubAppsServiceAddInfoPtr& sub_app : sub_apps) {
    GURL sub_app_install_url(sub_app->install_url);
    if (!url::IsSameOriginWith(sub_app_install_url, parent_app_url)) {
      std::move(result_callback).Run(/*mojom_results=*/{});
      ReportBadMessageAndDeleteThis(
          "Unexpected request: Add calls only supported for sub apps on the "
          "same origin as the calling app.");
      return;
    }

    if (!GURL(sub_app->unhashed_app_id).is_valid()) {
      std::move(result_callback).Run(/*mojom_results=*/{});
      ReportBadMessageAndDeleteThis("App ids must be valid URLs.");
      return;
    }
  }

  auto install_command = std::make_unique<SubAppInstallCommand>(
      *parent_app_id, InstallParamsFromMojo(std::move(sub_apps)),
      base::BindOnce(&OnAdd, std::move(result_callback)),
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext()),
      &provider->registrar(), &provider->install_finalizer(),
      std::make_unique<WebAppUrlLoader>(),
      std::make_unique<WebAppDataRetriever>());

  provider->command_manager().ScheduleCommand(std::move(install_command));
}

void SubAppsServiceImpl::List(ListCallback result_callback) {
  // Verify that the calling app is installed itself (cf. `Add`).
  const AppId* parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id) {
    return std::move(result_callback)
        .Run(SubAppsServiceListResult::New(SubAppsServiceResult::kFailure,
                                           std::vector<UnhashedAppId>()));
  }
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&SubAppsServiceImpl::List,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(result_callback)));
    return;
  }

  WebAppRegistrar& registrar = provider->registrar();

  std::vector<UnhashedAppId> sub_app_ids;
  for (const AppId& web_app_id : registrar.GetAllSubAppIds(*parent_app_id)) {
    const WebApp* web_app = registrar.GetAppById(web_app_id);
    sub_app_ids.push_back(
        GenerateAppIdUnhashed(web_app->manifest_id(), web_app->start_url()));
  }

  std::move(result_callback)
      .Run(SubAppsServiceListResult::New(SubAppsServiceResult::kSuccess,
                                         std::move(sub_app_ids)));
}

void SubAppsServiceImpl::Remove(const UnhashedAppId& unhashed_app_id,
                                RemoveCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&SubAppsServiceImpl::Remove,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  unhashed_app_id, std::move(result_callback)));
    return;
  }

  // Verify that the calling app is installed itself (cf. `Add`).
  const AppId* calling_app_id = GetAppId(render_frame_host());
  if (!calling_app_id) {
    return std::move(result_callback).Run(SubAppsServiceResult::kFailure);
  }

  // `unhashed_app_id` should form a proper URL
  // (https://www.w3.org/TR/appmanifest/#dfn-identity).
  if (!GURL(unhashed_app_id).is_valid()) {
    return std::move(result_callback).Run(SubAppsServiceResult::kFailure);
  }

  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_app_id);
  const WebApp* app = provider->registrar().GetAppById(sub_app_id);

  // Verify that the app we're trying to remove exists, that its parent_app is
  // the one doing the current call, and that the app was locally installed.
  if (!app || !app->parent_app_id() ||
      *calling_app_id != *app->parent_app_id() ||
      !app->is_locally_installed()) {
    return std::move(result_callback).Run(SubAppsServiceResult::kFailure);
  }

  provider->install_finalizer().UninstallExternalWebApp(
      sub_app_id, WebAppManagement::Type::kSubApp,
      webapps::WebappUninstallSource::kSubApp,
      base::BindOnce(&OnRemove, std::move(result_callback)));
}

}  // namespace web_app
