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

#include "base/check.h"
#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_page_contents_delegate.h"

namespace neva_app_runtime {

AuthChallengeInfo::AuthChallengeInfo() = default;
AuthChallengeInfo::AuthChallengeInfo(const AuthChallengeInfo&) = default;
AuthChallengeInfo& AuthChallengeInfo::operator=(AuthChallengeInfo&) = default;
AuthChallengeInfo::~AuthChallengeInfo() = default;

FaviconInfo::FaviconInfo() = default;
FaviconInfo::FaviconInfo(const FaviconInfo&) = default;
FaviconInfo& FaviconInfo::operator=(FaviconInfo&) = default;
FaviconInfo::~FaviconInfo() = default;

std::string ContentIconTypeToString(blink::mojom::FaviconIconType icon_type) {
  switch (icon_type) {
    case blink::mojom::FaviconIconType::kFavicon:
      return "favicon";
    case blink::mojom::FaviconIconType::kTouchIcon:
      return "touchicon";
    case blink::mojom::FaviconIconType::kTouchPrecomposedIcon:
      return "touchprecomposedicon";
    case blink::mojom::FaviconIconType::kInvalid:
      return "invalid";
  }
  NOTREACHED();
  return "invalid";
}

NewWindowInfo::NewWindowInfo() = default;
NewWindowInfo::NewWindowInfo(const NewWindowInfo&) = default;
NewWindowInfo& NewWindowInfo::operator=(NewWindowInfo&) = default;
NewWindowInfo::~NewWindowInfo() = default;

PageContentsDelegate::~PageContentsDelegate() {}

void PageContentsDelegate::EnterHtmlFullscreen() {}

void PageContentsDelegate::DidFailLoad(const std::string&,
                                       const std::string&,
                                       int) {}

void PageContentsDelegate::DidFinishLoad(const std::string&) {}

void PageContentsDelegate::DidStartLoading() {}

void PageContentsDelegate::DidStartNavigation(const std::string&) {}

void PageContentsDelegate::DidStopLoading() {}

void PageContentsDelegate::DidUpdateFaviconUrl(
    const std::vector<FaviconInfo>&) {}

void PageContentsDelegate::DOMReady() {}

void PageContentsDelegate::LeaveHtmlFullscreen() {}

void PageContentsDelegate::LoadProgressChanged(uint32_t) {}

void PageContentsDelegate::NavigationEntryCommitted() {}

void PageContentsDelegate::OnAcceptedLanguagesChanged(
    const std::string& languages) {}

void PageContentsDelegate::OnAuthChallenge(AuthChallengeInfo& challenge) {}

void PageContentsDelegate::OnClose() {}

void PageContentsDelegate::OnExit(const std::string&) {}

void PageContentsDelegate::OnFocusChanged(bool is_focused) {}

void PageContentsDelegate::OnNewWindowOpen(
    std::unique_ptr<PageContents> new_contents,
    NewWindowInfo& window_info) {}

void PageContentsDelegate::OnRendererUnresponsive() {}

void PageContentsDelegate::OnRendererResponsive() {}

void PageContentsDelegate::OnPermissionRequest(const std::string& permission,
                                               uint64_t id) {}

void PageContentsDelegate::OnVisibleRegionCaptured(
    const std::string& base64_data) {}

void PageContentsDelegate::OnZoomFactorChanged(double zoom_factor) {}

bool PageContentsDelegate::RunJSDialog(const std::string& type,
                                       const std::string& message) {
  return false;
}

void PageContentsDelegate::TitleUpdated(const std::string&) {}

void PageContentsDelegate::OnDestroying(PageContents*) {}

}  // namespace neva_app_runtime
