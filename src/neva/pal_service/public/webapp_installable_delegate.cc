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

namespace pal {

WebAppInstallableDelegate::WebAppInfo::~WebAppInfo() {}

WebAppInstallableDelegate::~WebAppInstallableDelegate() {}

WebAppInstallableDelegate::WebAppInfo
WebAppInstallableDelegate::GenerateAppInfo(
    const std::string& title,
    const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons,
    const GURL& start_url,
    absl::optional<SkColor> background_color) {
  WebAppInfo info;
  info.title_ = title;
  info.icons_ = icons;
  info.start_url_ = start_url;
  info.background_color_ = background_color;

  info.id_ = GenerateAppId(&info);
  return info;
}

}  // namespace pal
