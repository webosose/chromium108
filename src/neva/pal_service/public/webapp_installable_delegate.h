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
#include <memory>
#include <string>

#include "base/callback_forward.h"
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
    const std::string& version() const { return version_; }
    void set_version(const std::string& version) { version_ = version; }
    const std::map<SquareSizePx, SkBitmap>& icons() const { return icons_; }
    const GURL& start_url() const { return start_url_; }
    const absl::optional<SkColor>& background_color() const {
      return background_color_;
    }
    int64_t timestamp() const { return timestamp_; }

   private:
    friend class WebAppInstallableDelegate;

    std::string id_;
    std::string title_;
    std::string version_;
    std::map<SquareSizePx, SkBitmap> icons_;
    GURL start_url_;
    absl::optional<SkColor> background_color_;
    int64_t timestamp_ = -1;
  };

  virtual ~WebAppInstallableDelegate();

  virtual void UpdateApp() = 0;
  virtual bool SaveArtifacts(const WebAppInfo* app_info,
                             bool isUpdate = false) = 0;
  using ResultCallback = base::OnceCallback<void(bool result)>;
  virtual void IsWebAppForUrlInstalled(const GURL& app_start_url,
                                       ResultCallback) = 0;
  // Call ShouldAppForURLBeUpdated before download resources, will return false
  // when update is in process already, or it was updated not so long ago
  virtual bool ShouldAppForURLBeUpdated(const GURL& app_start_url,
                                        ResultCallback) = 0;
  using ResultWithVersionCallback =
      base::OnceCallback<void(bool result, const std::string& version)>;
  virtual void IsInfoChanged(std::unique_ptr<WebAppInfo> app_info,
                             ResultWithVersionCallback callback) = 0;

  std::unique_ptr<WebAppInstallableDelegate::WebAppInfo> GenerateAppInfo(
      const std::string& title,
      const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons,
      const GURL& start_url,
      absl::optional<SkColor> background_color);

 protected:
  virtual std::string GenerateAppId(const GURL& app_start_url) = 0;
};

}  // namespace pal

#endif  // NEVA_PAL_SERVICE_PUBLIC_WEBAPP_INSTALLABLE_DELEGATE_H_
