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
#include "neva/pal_service/luna/luna_client.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"

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

  bool SaveArtifacts(const WebAppInfo* app_info) override;
  bool IsWebAppInstalled(const WebAppInfo* app_info) override;

 protected:
  std::string GenerateAppId(const WebAppInfo* app_info) override;

 private:
  struct Icon {
    enum Type {
      REGULAR,
      MINI,
      LARGE,
      SPLASH,
    };

    Icon(Type type, const SkBitmap* bitmap);

    Type type_;
    const SkBitmap* bitmap_;
    std::string appinfo_key_;
    std::string file_name_;
  };

  static std::unique_ptr<luna::Client> InitLunaClient();

  void RegisterInstalledApp(const std::string& id);
  void OnScanApp(pal::luna::Client::ResponseStatus status,
                 unsigned token,
                 const std::string& json);
  bool WriteIcons(const base::FilePath& app_dir,
                  const std::vector<Icon>& icons);
  std::vector<Icon> SelectIcons(
      const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons);
  bool WriteIconToFile(const base::FilePath& file_path, const SkBitmap* bitmap);
  base::FilePath GetAppDir(const WebAppInfo* app_info);

  std::unique_ptr<luna::Client> luna_client_;
  base::WeakPtrFactory<WebAppInstallableDelegateWebOS> weak_ptr_factory_;
};

}  // namespace webos
}  // namespace pal

#endif  // NEVA_PAL_SERVICE_WEBOS_WEBAPP_INSTALLABLE_DELEGATE_WEBOS_H_