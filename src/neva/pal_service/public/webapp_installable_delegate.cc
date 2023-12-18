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

#include "neva/pal_service/public/webapp_installable_delegate.h"

#include "base/time/time.h"
#include "include/core/SkBitmap.h"
#include "neva/app_runtime/public/app_runtime_constants.h"
#include "ui/gfx/skia_util.h"

namespace pal {

WebAppInstallableDelegate::WebAppInfo::~WebAppInfo() {}

WebAppInstallableDelegate::~WebAppInstallableDelegate() {}

std::unique_ptr<WebAppInstallableDelegate::WebAppInfo>
WebAppInstallableDelegate::GenerateAppInfo(
    const std::string& title,
    const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons,
    const GURL& start_url,
    absl::optional<SkColor> background_color) {
  std::unique_ptr<WebAppInfo> info = std::make_unique<WebAppInfo>();
  info->timestamp_ = base::Time::Now().ToJavaTime();
  info->title_ = title;
  info->icons_ = icons;
  info->start_url_ = start_url;
  info->background_color_ = background_color;

  info->id_ = GenerateAppId(info->start_url());
  info->version_ = neva_app_runtime::kPwaInitVersion;
  return std::move(info);
}

}  // namespace pal
