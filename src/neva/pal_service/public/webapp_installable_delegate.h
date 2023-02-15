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

#ifndef NEVA_PAL_SERVICE_PUBLIC_WEBAPP_INSTALLABLE_DELEGATE_H_
#define NEVA_PAL_SERVICE_PUBLIC_WEBAPP_INSTALLABLE_DELEGATE_H_

#include <map>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace pal {

class WebAppInstallableDelegate {
 public:
  class WebAppInfo {
   public:
    using SquareSizePx = int;

    virtual ~WebAppInfo();

    const std::string& id() const { return id_; }
    const std::string& title() const { return title_; }
    const std::map<SquareSizePx, SkBitmap>& icons() const { return icons_; }
    const GURL& start_url() const { return start_url_; }
    const absl::optional<SkColor>& background_color() const {
      return background_color_;
    }

   private:
    friend class WebAppInstallableDelegate;

    std::string id_;
    std::string title_;
    std::map<SquareSizePx, SkBitmap> icons_;
    GURL start_url_;
    absl::optional<SkColor> background_color_;
  };

  virtual ~WebAppInstallableDelegate();

  virtual bool SaveArtifacts(const WebAppInfo* app_info) = 0;
  virtual bool IsWebAppInstalled(const WebAppInfo* app_info) = 0;

  WebAppInfo GenerateAppInfo(
      const std::string& title,
      const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons,
      const GURL& start_url,
      absl::optional<SkColor> background_color);

 protected:
  virtual std::string GenerateAppId(const WebAppInfo* app_info) = 0;
};

}  // namespace pal

#endif  // NEVA_PAL_SERVICE_PUBLIC_WEBAPP_INSTALLABLE_DELEGATE_H_
