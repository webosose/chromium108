// Copyright 2022 LG Electronics, Inc.
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

#include "neva/extensions/browser/neva_extensions_service_impl.h"

#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "neva/extensions/browser/api/extension_action/extension_action_api.h"
#include "neva/extensions/browser/neva_extensions_service_factory.h"
#include "neva/extensions/browser/tab_helper.h"
#include "neva/logging.h"
#include "ui/views/controls/webview/webview.h"

namespace neva {

NevaExtensionsServiceImpl::TabCreationRequest::TabCreationRequest() = default;

NevaExtensionsServiceImpl::TabCreationRequest::~TabCreationRequest() = default;

NevaExtensionsServiceImpl::NevaExtensionsServiceImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

NevaExtensionsServiceImpl::~NevaExtensionsServiceImpl() = default;

// static
void NevaExtensionsServiceImpl::BindForRenderer(
    int render_process_id,
    mojo::PendingReceiver<mojom::NevaExtensionsService> receiver) {
  auto* host = content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  NevaExtensionsServiceImpl* extensions_service =
      NevaExtensionsServiceFactory::GetService(host->GetBrowserContext());
  extensions_service->Bind(std::move(receiver));
}

void NevaExtensionsServiceImpl::Bind(
    mojo::PendingReceiver<mojom::NevaExtensionsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NevaExtensionsServiceImpl::SetTabHelper(
    std::unique_ptr<TabHelper> tab_helper) {
  tab_helper_ = std::move(tab_helper);
}

TabHelper* NevaExtensionsServiceImpl::GetTabHelper() {
  return tab_helper_.get();
}

void NevaExtensionsServiceImpl::OnExtensionTabCreationRequested(
    const std::string& url,
    TabCreatedCB callback) {
  VLOG(1) << __func__ << " url: " << url;

  int request_id = request_id_generator_.GetNext();
  tab_creation_requests_.emplace(request_id,
                                 std::make_unique<TabCreationRequest>());
  tab_creation_requests_[request_id]->url = url;
  tab_creation_requests_[request_id]->callback = std::move(callback);

  if (client_)
    RequestTabCreationToClient(request_id);
  else
    pending_tab_creation_requests_.emplace_back(request_id);
}

void NevaExtensionsServiceImpl::RequestTabCreationToClient(
    uint64_t request_id) {
  NEVA_DCHECK(client_);
  client_->OnExtensionTabCreationRequested(request_id);
}

void NevaExtensionsServiceImpl::OnExtensionTabCreated(uint64_t request_id,
                                                      uint64_t tab_id) {
  VLOG(1) << __func__ << " request_id: " << request_id
          << " / tab_id: " << tab_id;

  content::WebContents* web_contents =
      tab_helper_->GetWebContentsFromId(tab_id);
  web_contents->GetController().LoadURL(
      GURL(tab_creation_requests_[request_id]->url), content::Referrer(),
      ui::PAGE_TRANSITION_LINK, std::string());
  std::move(tab_creation_requests_[request_id]->callback).Run(tab_id);
  tab_creation_requests_.erase(request_id);
}

void NevaExtensionsServiceImpl::BindClient(
    mojo::PendingRemote<mojom::NevaExtensionsServiceClient> client) {
  VLOG(1) << __func__;

  client_.Bind(std::move(client));

  if (pending_tab_creation_requests_.size() > 0) {
    for (auto& request_id : pending_tab_creation_requests_)
      RequestTabCreationToClient(request_id);
    pending_tab_creation_requests_.clear();
  }
}

void NevaExtensionsServiceImpl::GetExtensionsInfo(
    GetExtensionsInfoCallback callback) {
  std::vector<base::Value> extension_infos;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context_);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();

  for (auto& extension : extensions) {
    base::DictionaryValue dict;
    dict.SetString("id", extension->id());
    dict.SetString("name", extension->name());
    extension_infos.push_back(std::move(dict));
  }

  std::move(callback).Run(std::move(extension_infos));
}

void NevaExtensionsServiceImpl::OnExtensionSelected(
    uint64_t tab_id,
    const std::string& extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(
              extension_id,
              extensions::ExtensionRegistry::IncludeFlag::EVERYTHING);
  const extensions::ActionInfo* action_info =
      extensions::ActionInfo::GetExtensionActionInfo(extension);
  if (action_info) {
    LOG(INFO) << __func__ << " Popup creation Requested.";
    popup_extension_id_ = extension_id;
    client_->OnExtensionPopupCreationRequested();
    return;
  }

  ExtensionActionAPI::Get(browser_context_)
      ->DispatchExtensionActionClicked(
          extension_id, tab_id, tab_helper_->GetWebContentsFromId(tab_id));
}

void NevaExtensionsServiceImpl::OnExtensionPopupViewCreated(
    uint64_t popup_view_id) {
  VLOG(1) << __func__ << " / popup_view_id: " << popup_view_id;

  std::string extension_id = popup_extension_id_;
  popup_extension_id_ = std::string();

  views::View* popup_view = tab_helper_->GetViewFromId(popup_view_id);
  // neva_app_runtime::PageView returns only views::View. So we need to cast.
  views::WebView* web_view = static_cast<views::WebView*>(popup_view);
  if (!web_view) {
    LOG(ERROR) << __func__ << " Cannot get WebView for extension popup.";
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(
              extension_id,
              extensions::ExtensionRegistry::IncludeFlag::EVERYTHING);
  const extensions::ActionInfo* action_info =
      extensions::ActionInfo::GetExtensionActionInfo(extension);

  scoped_refptr<content::SiteInstance> site_instance =
      extensions::ProcessManager::Get(browser_context_)
          ->GetSiteInstanceForURL(action_info->default_popup_url);

  popup_extension_host_ = std::make_unique<extensions::ExtensionHost>(
      extension, site_instance.get(), action_info->default_popup_url,
      extensions::mojom::ViewType::kExtensionPopup);
  popup_extension_host_->CreateRendererSoon();
  web_view->SetWebContents(popup_extension_host_->host_contents());
  web_view->AddObserver(this);
}

void NevaExtensionsServiceImpl::OnViewIsDeleting(views::View* observed_view) {
  popup_extension_host_.reset();
}

}  // namespace neva
