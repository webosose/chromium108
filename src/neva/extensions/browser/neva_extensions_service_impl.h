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

#ifndef NEVA_EXTENSIONS_BROWSER_NEVA_EXTENSIONS_SERVICE_IMPL_H_
#define NEVA_EXTENSIONS_BROWSER_NEVA_EXTENSIONS_SERVICE_IMPL_H_

#include "base/atomic_sequence_num.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "neva/extensions/common/mojom/neva_extensions_service.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class ExtensionHost;
}  // namespace extensions

namespace neva {
class TabHelper;

class NevaExtensionsServiceImpl : public KeyedService,
                                  public mojom::NevaExtensionsService,
                                  public views::ViewObserver {
 public:
  using TabCreatedCB = base::OnceCallback<void(int)>;

  explicit NevaExtensionsServiceImpl(content::BrowserContext* browser_context);
  NevaExtensionsServiceImpl(const NevaExtensionsServiceImpl&) = delete;
  NevaExtensionsServiceImpl& operator=(const NevaExtensionsServiceImpl&) =
      delete;
  ~NevaExtensionsServiceImpl() override;

  static void BindForRenderer(
      int render_process_id,
      mojo::PendingReceiver<mojom::NevaExtensionsService> receiver);

  void Bind(mojo::PendingReceiver<mojom::NevaExtensionsService> receiver);

  void SetTabHelper(std::unique_ptr<TabHelper> tab_helper);

  TabHelper* GetTabHelper();

  void OnExtensionTabCreationRequested(const std::string& url,
                                       TabCreatedCB callback);

  void RequestTabCreationToClient(uint64_t request_id);

  // mojom::NevaExtensionsService
  void BindClient(
      mojo::PendingRemote<mojom::NevaExtensionsServiceClient> client) override;
  void GetExtensionsInfo(GetExtensionsInfoCallback callback) override;
  void OnExtensionSelected(uint64_t tab_id,
                           const std::string& extension_id) override;
  void OnExtensionTabCreated(uint64_t request_id, uint64_t tab_id) override;
  void OnExtensionPopupViewCreated(uint64_t tab_id) override;

  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  struct TabCreationRequest {
    TabCreationRequest();
    ~TabCreationRequest();
    std::string url;
    TabCreatedCB callback;
  };

  mojo::ReceiverSet<mojom::NevaExtensionsService> receivers_;
  mojo::Remote<mojom::NevaExtensionsServiceClient> client_;
  content::BrowserContext* browser_context_;
  std::unique_ptr<TabHelper> tab_helper_;

  std::string popup_extension_id_;
  std::unique_ptr<extensions::ExtensionHost> popup_extension_host_;

  base::AtomicSequenceNumber request_id_generator_;
  std::vector<uint64_t> pending_tab_creation_requests_;
  std::map<uint64_t, std::unique_ptr<TabCreationRequest>>
      tab_creation_requests_;
};

}  // namespace neva

#endif  // NEVA_EXTENSIONS_BROWSER_NEVA_EXTENSIONS_SERVICE_IMPL_H_
