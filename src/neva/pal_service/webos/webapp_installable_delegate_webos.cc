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

#include "neva/pal_service/webos/webapp_installable_delegate_webos.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/browser/browser_task_traits.h"
#include "neva/pal_service/luna/luna_names.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"

namespace {

bool CreateTempAppDir(const std::string& app_id, base::FilePath* out_dir) {
  base::FilePath dir_temp;
  base::PathService::Get(base::DIR_TEMP, &dir_temp);

  base::FilePath::StringType dir_name = app_id + FILE_PATH_LITERAL("_");

  return base::CreateTemporaryDirInDir(dir_temp, dir_name, out_dir);
}

std::string BackgroundColorForWebosAppinfo(int webappinfo_color) {
  return base::StringPrintf("#%06X", webappinfo_color & 0xFFFFFF);
}

/*
 * The LunaUpdatePostpone class is needed to implement the deferred command for
 * appinstallService. When the application is closed, the
 * WebAppInstallableDelegateWebOS is destroyed along with it. We need to execute
 * the luna command after the application is closed, for this we create a luna
 * client wrapper class that executes the command, waits for a response from
 * appinstallService about updating the application to then delete the class
 * instance from memory.
 */
class LunaUpdatePostpone {
 public:
  LunaUpdatePostpone(std::unique_ptr<pal::luna::Client>& luna_client_);
  ~LunaUpdatePostpone() = default;

  void WrapperCall(std::string&& service_uri, std::string&& call_params_str);

 private:
  void OnWrapperCall(pal::luna::Client::ResponseStatus status,
                     unsigned,
                     const std::string& json);

  std::unique_ptr<pal::luna::Client> luna_client_;
};

LunaUpdatePostpone::LunaUpdatePostpone(
    std::unique_ptr<pal::luna::Client>& luna_client)
    : luna_client_{std::move(luna_client)} {}

void LunaUpdatePostpone::WrapperCall(std::string&& service_uri,
                                     std::string&& call_params_str) {
  if (luna_client_ && luna_client_->IsInitialized()) {
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << " "
            << service_uri << " " << call_params_str;

    luna_client_->Subscribe(
        std::move(service_uri), std::move(call_params_str),
        base::BindRepeating(&LunaUpdatePostpone::OnWrapperCall,
                            base::Unretained(this)));

  } else {
    LOG(ERROR) << __func__ << "() Luna client not ready";
  }
}

void LunaUpdatePostpone::OnWrapperCall(pal::luna::Client::ResponseStatus status,
                                       unsigned,
                                       const std::string& json) {
  VLOG(1) << __func__ << "() status=" << static_cast<int>(status)
          << ", response='" << json << "'";

  auto vals(base::JSONReader::Read(json));
  if (vals && vals->is_dict()) {
    int progress = vals->FindIntPath("details.progress").value_or(0);
    int error = vals->FindIntPath("details.errorCode").value_or(0);
    if (progress == 100 || error != 0)
      delete this;
  }
}

}  // namespace

namespace pal {
namespace webos {

WebAppInstallableDelegateWebOS::Icon::Icon(Type type,
                                           const SkBitmap* bitmap,
                                           int64_t timestamp)
    : type_(type), bitmap_(bitmap) {
  std::string timestamp_string =
      timestamp > 0 ? "_" + std::to_string(timestamp) : "";
  appinfo_key_ = WebAppInstallableDelegateWebOS::Icon::icon_types.at(type);
  switch (type) {
    case Type::REGULAR:
      file_name_ = "icon" + timestamp_string + ".png";
      break;
    case Type::MINI:
      file_name_ = "icon-mini" + timestamp_string + ".png";
      break;
    case Type::LARGE:
      file_name_ = "icon-large" + timestamp_string + ".png";
      break;
    case Type::SPLASH:
      file_name_ = "icon-splash" + timestamp_string + ".png";
      break;
  }
}

WebAppInstallableDelegateWebOS::WebAppInstallableDelegateWebOS()
    : luna_client_(InitLunaClient()),
      haveUpdate{false},
      weak_ptr_factory_(this) {}

// static
std::unique_ptr<luna::Client> WebAppInstallableDelegateWebOS::InitLunaClient() {
  luna::Client::Params params;
  params.name = luna::GetServiceNameWithRandSuffix(
      luna::service_name::kChromiumInstallableManager);
  return luna::CreateClient(params);
}

void WebAppInstallableDelegateWebOS::IsWebAppForUrlInstalled(
    const GURL& app_start_url,
    ResultCallback callback) {
  base::Value::Dict call_params;
  call_params.Set("id", GenerateAppId(app_start_url));
  std::string call_params_str;
  if (!base::JSONWriter::Write(call_params, &call_params_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize luna call params";
    return;
  }

  if (luna_client_ && luna_client_->IsInitialized()) {
    std::string service_uri = pal::luna::GetServiceURI(
        pal::luna::service_uri::kApplicationManager, "getAppInfo");
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << " "
            << service_uri << " " << call_params_str;
    luna_client_->Call(
        std::move(service_uri), std::move(call_params_str),
        base::BindOnce(&WebAppInstallableDelegateWebOS::OnGetAppInfoStatus,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    LOG(ERROR) << __func__ << "() Luna client not ready";
  }
}

void WebAppInstallableDelegateWebOS::OnGetAppInfoStatus(
    ResultCallback callback,
    pal::luna::Client::ResponseStatus status,
    unsigned token,
    const std::string& json) {
  if (status == pal::luna::Client::ResponseStatus::SUCCESS) {
    absl::optional<base::Value> vals = base::JSONReader::Read(json);
    if (vals && vals->is_dict()) {
      absl::optional<bool> return_value = vals->FindBoolKey("returnValue");
      std::move(callback).Run(return_value.has_value() && return_value.value());
      return;
    }
  }
  std::move(callback).Run(false);
}

bool WebAppInstallableDelegateWebOS::ShouldAppForURLBeUpdated(
    const GURL& app_start_url,
    ResultCallback callback) {
  std::string app_id = GenerateAppId(app_start_url);
  std::move(callback).Run(true);
  return true;
}

// Put app contents in a temporary directory and ask appinstall to update
bool WebAppInstallableDelegateWebOS::SaveArtifacts(const WebAppInfo* app_info,
                                                   bool isUpdate) {
  base::FilePath app_dir;
  if (!CreateTempAppDir(app_info->id(), &app_dir)) {
    LOG(ERROR) << __func__ << "() Failed to create temporary webapp directory";
    return false;
  }
  VLOG(1) << __func__ << "() Created temporary webapp directory: " << app_dir;

  auto selected_icons = SelectIcons(app_info);
  if (!WriteIcons(app_dir, selected_icons)) {
    LOG(ERROR) << __func__ << "() Failed to write icons";
    return false;
  }

  base::Value::Dict appinfo_content;
  appinfo_content.Set("type", "web");
  appinfo_content.Set("id", app_info->id());
  appinfo_content.Set("version", app_info->version());
  appinfo_content.Set("title", app_info->title());
  appinfo_content.Set("main", app_info->start_url().spec());
  appinfo_content.Set("icon", "icon.png");
  appinfo_content.Set("trustLevel", "default");
  appinfo_content.Set("disallowScrollingInMainFrame", false);
  for (const auto& icon : selected_icons) {
    appinfo_content.Set(icon.appinfo_key_, icon.file_name_);
  }
  if (app_info->background_color().has_value()) {
    appinfo_content.Set("bgColor", BackgroundColorForWebosAppinfo(
                                       app_info->background_color().value()));
  }

  std::string appinfo_str;
  if (!base::JSONWriter::WriteWithOptions(
          appinfo_content, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &appinfo_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize appinfo.json";
    return false;
  }

  base::FilePath appinfo_path = app_dir.AppendASCII("appinfo.json");
  if (!base::WriteFile(appinfo_path, appinfo_str.c_str(), appinfo_str.size())) {
    LOG(ERROR) << __func__ << "() Failed to write " << appinfo_path;
    return false;
  }
  app_id_ = app_info->id();
  app_dir_ = app_dir.value();
  haveUpdate = isUpdate;

  if (!isUpdate)
    CallAppInstall();

  return true;
}

void WebAppInstallableDelegateWebOS::UpdateApp() {
  if (!haveUpdate)
    return;

  if (app_id_.empty() || app_dir_.empty())
    return;

  base::Value::Dict call_params;
  call_params.Set("id", app_id_);
  call_params.Set("ipkUrl", neva_app_runtime::kPwaSchemaName + app_dir_);
  call_params.Set("subscribe", true);
  std::string call_params_str;
  if (!base::JSONWriter::Write(call_params, &call_params_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize luna call params";
    return;
  }

  std::string service_uri = pal::luna::GetServiceURI(
      pal::luna::service_uri::kAppInstallService, "install");

  auto l = InitLunaClient();
  LunaUpdatePostpone* lw = new LunaUpdatePostpone(l);
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LunaUpdatePostpone::WrapperCall, base::Unretained(lw),
                     std::move(service_uri), std::move(call_params_str)),
      base::Milliseconds(neva_app_runtime::kPwaUpdateTimeout));
}

void WebAppInstallableDelegateWebOS::CallAppInstall() {
  base::Value::Dict call_params;
  call_params.Set("id", app_id_);
  call_params.Set("ipkUrl", neva_app_runtime::kPwaSchemaName + app_dir_);
  std::string call_params_str;
  if (!base::JSONWriter::Write(call_params, &call_params_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize luna call params";
    return;
  }

  if (luna_client_ && luna_client_->IsInitialized()) {
    std::string service_uri = pal::luna::GetServiceURI(
        pal::luna::service_uri::kAppInstallService, "install");
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << " "
            << service_uri << " " << call_params_str;
    luna_client_->Call(
        std::move(service_uri), std::move(call_params_str),
        base::BindOnce(&WebAppInstallableDelegateWebOS::OnInstallApp,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << __func__ << "() Luna client not ready";
  }
}

void WebAppInstallableDelegateWebOS::OnInstallApp(
    pal::luna::Client::ResponseStatus status,
    unsigned,
    const std::string& json) {
  VLOG(1) << __func__ << "() status=" << static_cast<int>(status)
          << ", response='" << json << "'";
}

bool WebAppInstallableDelegateWebOS::WriteIcons(
    const base::FilePath& app_dir,
    const std::vector<WebAppInstallableDelegateWebOS::Icon>& icons) {
  for (const auto& icon : icons) {
    if (!WriteIconToFile(app_dir.AppendASCII(icon.file_name_), icon.bitmap_))
      return false;
  }
  return true;
}

bool WebAppInstallableDelegateWebOS::WriteIconToFile(
    const base::FilePath& file_path,
    const SkBitmap* bitmap) {
  std::vector<unsigned char> image_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(*bitmap, false, &image_data)) {
    LOG(ERROR) << __func__ << "() Could not encode icon data for file "
               << file_path.AsUTF8Unsafe();
    return false;
  }

  const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
  int size = base::checked_cast<int>(image_data.size());
  if (base::WriteFile(file_path, image_data_ptr, size) != size) {
    LOG(ERROR) << __func__
               << "() Could not write icon file: " << file_path.AsUTF8Unsafe();
    return false;
  }
  return true;
}

std::vector<WebAppInstallableDelegateWebOS::Icon>
WebAppInstallableDelegateWebOS::SelectIcons(const WebAppInfo* app_info) {
  const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons = app_info->icons();
  std::vector<Icon> selected;
  if (icons.empty())
    return selected;

  auto comp = [](const std::pair<WebAppInfo::SquareSizePx, SkBitmap> el,
                 WebAppInfo::SquareSizePx w) { return el.first < w; };

  auto iter_regular = std::lower_bound(icons.cbegin(), icons.cend(), 80, comp);
  auto iter_mini =
      (iter_regular == icons.cbegin()) ? icons.cend() : icons.cbegin();
  auto range_start = iter_regular;
  if (range_start != icons.cend())
    ++range_start;

  auto iter_large = icons.cend();
  if (range_start != icons.cend()) {
    iter_large = std::lower_bound(range_start, icons.cend(), 130, comp);
    range_start = iter_large;
    if (range_start != icons.cend())
      ++range_start;
  }

  auto iter_splash = icons.cend();
  if (range_start != icons.cend()) {
    iter_splash = std::lower_bound(range_start, icons.cend(), 256, comp);
  }

  selected.emplace_back(Icon::Type::REGULAR, &(iter_regular->second),
                        app_info->timestamp());
  if (iter_mini != icons.cend())
    selected.emplace_back(Icon::Type::MINI, &(iter_mini->second),
                          app_info->timestamp());
  if (iter_large != icons.cend())
    selected.emplace_back(Icon::Type::LARGE, &(iter_large->second),
                          app_info->timestamp());
  if (iter_splash != icons.cend())
    selected.emplace_back(Icon::Type::SPLASH, &(iter_splash->second),
                          app_info->timestamp());
  return selected;
}

bool WebAppInstallableDelegateWebOS::AreWebosIconsDifferent(
    const std::vector<Icon>& new_icons,
    const base::Value& appinfo_json,
    const base::FilePath& app_dir) {
  for (const auto& icon_type_and_name : Icon::icon_types) {
    const auto& icon_type = icon_type_and_name.first;
    const auto& icon_name = icon_type_and_name.second;

    // Find WebOS icon
    const std::string* icon_filename;
    icon_filename = appinfo_json.FindStringKey(icon_name);
    bool webos_has_icon_for_type = !!icon_filename;

    // Find new app info icon
    auto new_icon = std::find_if(new_icons.rbegin(), new_icons.rend(),
                                 [icon_type](const auto& icon_pair) {
                                   return icon_pair.type_ == icon_type;
                                 });
    bool new_app_has_icon_for_type = new_icon != new_icons.rend();

    if (webos_has_icon_for_type != new_app_has_icon_for_type) {
      // TODO add __func__ to all LOGs
      VLOG(1) << "WebOS does" << (webos_has_icon_for_type ? "" : " not")
              << " have and new app_info does"
              << (new_app_has_icon_for_type ? "" : " not") << " have '"
              << icon_name << "' icon";
      return true;
    }

    if (webos_has_icon_for_type) {
      // Both new app and webos icons are found, now compare their bitmaps
      base::FilePath icon_path = app_dir.Append(*icon_filename);

      std::string icon_data;
      if (base::ReadFileToString(icon_path, &icon_data)) {
        SkBitmap webos_bitmap;
        if (!gfx::PNGCodec::Decode(
                reinterpret_cast<const unsigned char*>(icon_data.c_str()),
                icon_data.size(), &webos_bitmap)) {
          LOG(ERROR) << "Cannot decode icon from " << icon_path;
          return true;
        }

        if (!gfx::BitmapsAreEqual(webos_bitmap, *new_icon->bitmap_)) {
          VLOG(1) << "Bitmaps for " << icon_name << " differs";
          return true;
        }
      } else {
        // Cannot read the icon form disk
        LOG(ERROR) << "Cannot read file " << icon_path;
        return true;
      }
    }
  }

  return false;
}

const std::map<WebAppInstallableDelegateWebOS::Icon::Type, std::string>
    WebAppInstallableDelegateWebOS::Icon::icon_types = {
        {WebAppInstallableDelegateWebOS::Icon::Type::REGULAR, "icon"},
        {WebAppInstallableDelegateWebOS::Icon::Type::MINI, "miniicon"},
        {WebAppInstallableDelegateWebOS::Icon::Type::LARGE, "largeIcon"},
        {WebAppInstallableDelegateWebOS::Icon::Type::SPLASH, "splashicon"}};

void WebAppInstallableDelegateWebOS::IsInfoChanged(
    std::unique_ptr<WebAppInfo> fresh_app_info,
    ResultWithVersionCallback callback) {
  VLOG(1) << "Start checking current WebOS appinfo for: "
          << fresh_app_info->start_url();

  base::Value::Dict call_params;
  call_params.Set("id", fresh_app_info->id());
  std::string call_params_str;
  // TODO(pwa): Need to use the new API when available
  if (!base::JSONWriter::Write(call_params, &call_params_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize luna call params";
    return;
  }

  if (luna_client_ && luna_client_->IsInitialized()) {
    std::string service_uri = pal::luna::GetServiceURI(
        pal::luna::service_uri::kApplicationManager, "getAppInfo");
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << " "
            << service_uri << " " << call_params_str;
    luna_client_->Call(
        std::move(service_uri), std::move(call_params_str),
        base::BindOnce(&WebAppInstallableDelegateWebOS::OnGetAppInfoPath,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(fresh_app_info), std::move(callback)));
  } else {
    LOG(ERROR) << __func__ << "() Luna client not ready";
  }
}

void WebAppInstallableDelegateWebOS::OnGetAppInfoPath(
    std::unique_ptr<WebAppInfo> fresh_app_info,
    ResultWithVersionCallback callback,
    pal::luna::Client::ResponseStatus status,
    unsigned token,
    const std::string& json) {
  if (status != pal::luna::Client::ResponseStatus::SUCCESS) {
    LOG(ERROR) << __func__ << "() No response from the service";
    std::move(callback).Run(false, fresh_app_info->version());
    return;
  }

  auto vals = base::JSONReader::Read(json);
  if (!vals || !vals->is_dict()) {
    LOG(ERROR) << __func__ << "() Invalid response format";
    std::move(callback).Run(false, fresh_app_info->version());
    return;
  }

  const std::string* folderPath = vals->FindStringPath("appInfo.folderPath");
  if (!folderPath || folderPath->empty()) {
    LOG(ERROR) << __func__ << "() Invalid appInfo.folderPath attribute";
    std::move(callback).Run(false, fresh_app_info->version());
    return;
  }

  base::FilePath app_dir(*folderPath);
  if (!base::DirectoryExists(app_dir)) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  std::string app_info_str;
  if (!base::ReadFileToString(app_dir.Append("appinfo.json"), &app_info_str)) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  auto current_appinfo_json = base::JSONReader::Read(app_info_str);
  if (!current_appinfo_json || !current_appinfo_json->is_dict()) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  const std::string* current_ver = vals->FindStringPath("appInfo.version");
  if (current_ver)
    fresh_app_info->set_version(GenerateAppVersion(*current_ver));

  // TODO: maybe return false up to this point
  VLOG(1) << "Found current WebOS appinfo for: " << fresh_app_info->start_url();

  std::string* current_id = current_appinfo_json->FindStringKey("id");

  if (current_id) {
    VLOG(1) << "Checking ID old/new: " << *current_id << "/"
            << fresh_app_info->id();
  }

  if (!current_id || *current_id != fresh_app_info->id()) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  std::string* current_title = current_appinfo_json->FindStringKey("title");
  if (current_title) {
    VLOG(1) << "Checking Title old/new: " << *current_title << "/"
            << fresh_app_info->title();
  }

  if (!current_title || *current_title != fresh_app_info->title()) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  std::string* current_url = current_appinfo_json->FindStringKey("main");
  if (!current_url) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  if (GURL(*current_url) != fresh_app_info->start_url()) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  // background_color is absl::optional<SkColor> in WebAppInfo
  std::string* background_color =
      current_appinfo_json->FindStringKey("bgColor");
  bool has_bgcolor_in_current = !!background_color;
  bool has_bgcolor_in_fresh = fresh_app_info->background_color().has_value();

  if (has_bgcolor_in_current != has_bgcolor_in_fresh) {
    return std::move(callback).Run(true, fresh_app_info->version());
  }

  if (has_bgcolor_in_fresh) {
    if (BackgroundColorForWebosAppinfo(
            fresh_app_info->background_color().value()) != *background_color) {
      return std::move(callback).Run(true, fresh_app_info->version());
    }
  }
  //  Lastly compare icons
  auto new_icons = SelectIcons(fresh_app_info.get());
  return std::move(callback).Run(
      AreWebosIconsDifferent(new_icons, current_appinfo_json.value(), app_dir),
      fresh_app_info->version());
}

std::string WebAppInstallableDelegateWebOS::GenerateAppId(
    const GURL& app_start_url) {
  std::vector<std::string> parts =
      base::SplitString(app_start_url.host(), ".", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  std::string id = neva_app_runtime::kPwaAppNamePrefix;
  id.pop_back();
  for (auto it = parts.crbegin(); it != parts.crend(); ++it) {
    id.append(".").append(*it);
  }
  return id;
}

std::string WebAppInstallableDelegateWebOS::GenerateAppVersion(
    const std::string& old_version) {
  base::Version vers{old_version};
  if (!vers.IsValid() || vers.components().size() != 3)
    return neva_app_runtime::kPwaInitVersion;

  if ((vers.components()[2] + 1 >= neva_app_runtime::kPwaMaxVersionNumber))
    return neva_app_runtime::kPwaInitVersion;

  return base::Version{{1, 0, vers.components()[2] + 1}}.GetString();
}

}  // namespace webos
}  // namespace pal
