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

#include "neva/app_runtime/browser/app_runtime_web_contents_delegate.h"

#include "base/command_line.h"
#include "base/neva/base_switches.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents.h"
#include "neva/pal_service/pal_platform_factory.h"
#include "neva/pal_service/public/notification_manager_delegate.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace neva_app_runtime {

AppRuntimeWebContentsDelegate::AppRuntimeWebContentsDelegate() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNotificationForUnsupportedFeatures)) {
    notification_manager_delegate_ =
        pal::PlatformFactory::Get()->CreateNotificationManagerDelegate();
  }
}

AppRuntimeWebContentsDelegate::~AppRuntimeWebContentsDelegate() = default;

void AppRuntimeWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
  if (notification_manager_delegate_) {
    std::string app_id =
        content::WebContents::FromRenderFrameHost(render_frame_host)
            ->GetMutableRendererPrefs()
            ->application_id;
    notification_manager_delegate_->CreateToast(
        app_id, "File selection is not supported.");
  }
}

void AppRuntimeWebContentsDelegate::SetSSLCertErrorPolicy(
    SSLCertErrorPolicy policy) {}

SSLCertErrorPolicy AppRuntimeWebContentsDelegate::GetSSLCertErrorPolicy()
    const {
  return SSL_CERT_ERROR_POLICY_DEFAULT;
}

}  // namespace neva_app_runtime
