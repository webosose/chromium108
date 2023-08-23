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

#ifndef NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PAGE_CONTENTS_IMPL_H_
#define NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PAGE_CONTENTS_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "neva/app_runtime/app/app_runtime_page_contents_delegate.h"
#include "neva/browser_shell/service/public/mojom/browser_shell_page_contents.mojom.h"

namespace neva_app_runtime {
class PageContents;
}

namespace browser_shell {

class PageContentsImpl : public mojom::PageContents,
                         public neva_app_runtime::PageContentsDelegate {
 public:
  PageContentsImpl(neva_app_runtime::PageContents* page_contents);
  PageContentsImpl(uint64_t id);
  PageContentsImpl(const PageContentsImpl&) = delete;
  PageContentsImpl& operator=(const PageContentsImpl&) = delete;
  ~PageContentsImpl() override;

  bool GetActiveState();
  uint64_t GetID() const;
  std::string GetUserAgent() const;
  double GetZoomFactor() const;
  neva_app_runtime::PageContents* GetPageContents() const;
  mojo::PendingAssociatedReceiver<mojom::PageContentsClient>
  BindNewEndpointAndPassReceiver();

  // mojom::PageContents
  void BindClient(BindClientCallback callback) override;
  void Activate() override;
  void AckPermission(bool ack, uint64_t id) override;
  void AckAuthChallenge(const std::string& login,
                        const std::string& passwd,
                        const std::string& url) override;
  void CaptureVisibleRegion(const std::string& format,
                            int32_t quality,
                            CaptureVisibleRegionCallback callback) override;
  void BindNewPageContentsById(
      mojo::PendingReceiver<mojom::PageContents> receiver,
      uint64_t id,
      BindNewPageContentsByIdCallback callback) override;
  void ClearData(const std::string& clear_options,
                 const std::string& clear_data_type_set) override;
  void Deactivate() override;
  void SyncId(SyncIdCallback callback) override;
  void CloseJSDialog(bool success, const std::string& response) override;
  void ExecuteJavaScriptInAllFrames(const std::string& code) override;
  void ExecuteJavaScriptInMainFrame(const std::string& code) override;
  void SyncActiveState(SyncActiveStateCallback callback) override;
  void GoBack() override;
  void GoForward() override;
  void LoadURL(const std::string& url, LoadURLCallback callback) override;
  void Reload() override;
  void ResumeDOM() override;
  void ResumeMedia() override;
  void SetAcceptedLanguages(const std::string& languages) override;
  void SetErrorPageHiding(bool enable) override;
  void SetFocus() override;
  void SetPageBaseBackgroundColor(const std::string& color) override;
  void SetSuppressSSLErrorPolicy(bool suppress_errors) override;
  void SetZoomFactor(double factor) override;
  void Stop() override;
  void SuspendDOM() override;
  void SuspendMedia() override;

  // neva_app_runtime::PageContentsDelegate
  void EnterHtmlFullscreen() override;
  void DidFailLoad(const std::string& url,
                   const std::string& error,
                   int error_code) override;
  void DidFinishLoad(const std::string& url) override;
  void DidStartLoading() override;
  void DidStartNavigation(const std::string& url) override;
  void DidStopLoading() override;
  void DidUpdateFaviconUrl(
      const std::vector<neva_app_runtime::FaviconInfo>& info) override;
  void DOMReady() override;
  void LeaveHtmlFullscreen() override;
  void LoadProgressChanged(uint32_t progress) override;
  void NavigationEntryCommitted() override;
  void OnAcceptedLanguagesChanged(const std::string& languages) override;
  void OnAuthChallenge(neva_app_runtime::AuthChallengeInfo& challenge) override;
  void OnClose() override;
  void OnExit(const std::string& reason) override;
  void OnFocusChanged(bool is_focused) override;
  void OnNewWindowOpen(
      std::unique_ptr<neva_app_runtime::PageContents> new_contents,
      neva_app_runtime::NewWindowInfo& window_info) override;
  void OnRendererUnresponsive() override;
  void OnRendererResponsive() override;
  void OnPermissionRequest(const std::string& permission, uint64_t id) override;
  void OnVisibleRegionCaptured(const std::string& base64_data) override;
  void OnZoomFactorChanged(double zoom_factor) override;
  bool RunJSDialog(const std::string& type,
                   const std::string& message) override;
  void TitleUpdated(const std::string& title) override;
  void OnDestroying(neva_app_runtime::PageContents* contents) override;

  bool GetErrorPageHiding() const;

 private:
  neva_app_runtime::PageContents* page_contents_;
  mojo::AssociatedRemote<mojom::PageContentsClient> remote_client_;
  std::unique_ptr<CaptureVisibleRegionCallback>
      capture_visible_region_callback_;
};

}  // namespace browser_shell

#endif  // NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PAGE_CONTENTS_IMPL_H_
