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

#ifndef NEVA_PAL_SERVICE_WEBOS_WEBAPP_INSTALLABLE_DELEGATE_WEBOS_H_
#define NEVA_PAL_SERVICE_WEBOS_WEBAPP_INSTALLABLE_DELEGATE_WEBOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "neva/app_runtime/public/app_runtime_constants.h"
#include "neva/pal_service/luna/luna_client.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace pal {
namespace webos {

class WebAppInstallableDelegateWebOS : public WebAppInstallableDelegate {
 public:
  WebAppInstallableDelegateWebOS();
  ~WebAppInstallableDelegateWebOS() override = default;
  WebAppInstallableDelegateWebOS(const WebAppInstallableDelegateWebOS&) =
      delete;
  WebAppInstallableDelegateWebOS& operator=(
      const WebAppInstallableDelegateWebOS&) = delete;

  bool SaveArtifacts(const WebAppInfo* app_info,
                     bool isUpdate = false) override;
  void IsWebAppForUrlInstalled(const GURL& url,
                               ResultCallback callback) override;
  bool ShouldAppForURLBeUpdated(const GURL& app_start_url,
                                ResultCallback callback) override;
  void IsInfoChanged(std::unique_ptr<WebAppInfo> app_info,
                     ResultWithVersionCallback callback) override;
  void UpdateApp() override;

 protected:
  std::string GenerateAppId(const GURL& app_start_url) override;
  std::string GenerateAppVersion(const std::string& app_start_url);

 private:
  struct Icon {
    enum Type {
      REGULAR,
      MINI,
      LARGE,
      SPLASH,
    };

    Icon(Type type, const SkBitmap* bitmap, int64_t timestamp = 0);

    Type type_;
    const SkBitmap* bitmap_;
    std::string appinfo_key_;
    std::string file_name_;

    static const std::map<Icon::Type, std::string> icon_types;
  };

  static std::unique_ptr<luna::Client> InitLunaClient();

  void CallAppInstall();
  void OnGetAppInfoStatus(ResultCallback callback,
                          pal::luna::Client::ResponseStatus status,
                          unsigned token,
                          const std::string& json);
  void OnGetAppInfoPath(std::unique_ptr<WebAppInfo> fresh_app_info,
                        ResultWithVersionCallback callback,
                        pal::luna::Client::ResponseStatus status,
                        unsigned token,
                        const std::string& json);
  void OnInstallApp(pal::luna::Client::ResponseStatus status,
                    unsigned token,
                    const std::string& json);
  bool WriteIcons(const base::FilePath& app_dir,
                  const std::vector<Icon>& icons);
  bool WriteIconToFile(const base::FilePath& file_path, const SkBitmap* bitmap);
  std::vector<Icon> SelectIcons(const WebAppInfo* app_info);
  bool AreWebosIconsDifferent(const std::vector<Icon>& new_icons,
                              const base::Value& appinfo_json,
                              const base::FilePath& app_dir);

  std::unique_ptr<luna::Client> luna_client_;
  std::string app_id_;
  std::string app_dir_;
  bool haveUpdate;
  base::WeakPtrFactory<WebAppInstallableDelegateWebOS> weak_ptr_factory_;
};

}  // namespace webos
}  // namespace pal

#endif  // NEVA_PAL_SERVICE_WEBOS_WEBAPP_INSTALLABLE_DELEGATE_WEBOS_H_