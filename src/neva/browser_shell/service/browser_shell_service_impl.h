// Copyright 2021 LG Electronics, Inc.
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

#ifndef NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_SERVICE_IMPL_H_
#define NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_SERVICE_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "neva/browser_shell/service/browser_shell_ipc.h"
#include "neva/browser_shell/service/public/mojom/browser_shell_ipc_endpoint.mojom.h"
#include "neva/browser_shell/service/public/mojom/browser_shell_service.mojom.h"
#include "neva/browser_shell/service/public/mojom/browser_shell_webrequest.mojom.h"

namespace neva_app_runtime {
class Shell;
}

namespace browser_shell {

class ShellServiceImpl : public mojom::ShellService {
 public:
  explicit ShellServiceImpl(std::unique_ptr<neva_app_runtime::Shell> shell);
  ShellServiceImpl(const ShellServiceImpl&) = delete;
  ShellServiceImpl& operator=(const ShellServiceImpl&) = delete;
  ~ShellServiceImpl() override;

  void AddBinding(mojo::PendingReceiver<mojom::ShellService> receiver) override;

  void BindRemoteClient(
      mojo::PendingRemote<mojom::ShellServiceClient> client_remote) override;

  void BindShellWindow(mojo::PendingReceiver<mojom::ShellWindow> receiver,
                       BindShellWindowCallback callback) override;

  void CreatePageView(mojo::PendingReceiver<mojom::PageView> receiver,
                      const std::string& json,
                      CreatePageViewCallback callback) override;

  void CreatePageContents(mojo::PendingReceiver<mojom::PageContents> receiver,
                          const std::string& json,
                          CreatePageContentsCallback callback) override;

  void CreateShellIpcEndpoint(
      mojo::PendingReceiver<mojom::ShellIpcEndpoint> receiver,
      const std::string& name) override;

  void CreateWebRequest(mojo::PendingReceiver<mojom::WebRequest> receiver,
                        const std::string& partition) override;
 private:
  ShellIpc shell_ipc_;
  std::unique_ptr<neva_app_runtime::Shell> shell_;
  mojo::RemoteSet<mojom::ShellServiceClient> remotes_;
  mojo::ReceiverSet<mojom::ShellService> receivers_;
  mojo::UniqueReceiverSet<mojom::ShellWindow> shell_window_receivers_;
  mojo::UniqueReceiverSet<mojom::WebRequest> webrequest_receivers_;
};

}  // namespace browser_shell

#endif  // NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_SERVICE_IMPL_H_
