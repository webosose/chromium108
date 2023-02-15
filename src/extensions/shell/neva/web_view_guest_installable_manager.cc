// Copyright 2023 LG Electronics, Inc.
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

#include "extensions/shell/neva/web_view_guest_installable_manager.h"

#include "base/bind.h"
#include "base/callback.h"

namespace neva_app_runtime {

WebViewGuestInstallableManager::WebViewGuestInstallableManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebViewGuestInstallableManager>(
          *web_contents),
      web_contents_(web_contents),
      receivers_(web_contents, this),
      weak_factory_(this) {}

WebViewGuestInstallableManager::~WebViewGuestInstallableManager() {}

// static
void WebViewGuestInstallableManager::BindInstallableManager(
    mojo::PendingAssociatedReceiver<mojom::InstallableManager> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* installable_manager =
      WebViewGuestInstallableManager::FromWebContents(web_contents);
  if (!installable_manager)
    return;

  installable_manager->receivers_.Bind(rfh, std::move(receiver));
}

void WebViewGuestInstallableManager::GetInfo(GetInfoCallback callback) {
  installable_manager_.CheckInstallability(
      web_contents_,
      base::BindOnce(&WebViewGuestInstallableManager::OnGetInfo,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebViewGuestInstallableManager::OnGetInfo(GetInfoCallback callback,
                                               bool installable,
                                               bool installed) {
  std::move(callback).Run(installable, installed);
}

void WebViewGuestInstallableManager::InstallApp(InstallAppCallback callback) {
  installable_manager_.InstallWebApp(
      web_contents_,
      base::BindOnce(&WebViewGuestInstallableManager::OnInstallApp,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebViewGuestInstallableManager::OnInstallApp(InstallAppCallback callback,
                                                  bool success) {
  std::move(callback).Run(success);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebViewGuestInstallableManager);

}  // namespace neva_app_runtime
