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

#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/neva/base_switches.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "neva/pal_service/luna/luna_names.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"

namespace {

bool CreateTempAppdir(const std::string& app_id, base::FilePath* out_dir) {
  base::FilePath dir_temp;
  base::PathService::Get(base::DIR_TEMP, &dir_temp);

  base::FilePath::StringType dir_name = app_id + FILE_PATH_LITERAL("_");

  return base::CreateTemporaryDirInDir(dir_temp, dir_name, out_dir);
}

std::string BackgroundColorForWebosAppinfo(int webappinfo_color) {
  return base::StringPrintf("#%06X", webappinfo_color & 0xFFFFFF);
}
}  // namespace

namespace pal {
namespace webos {

WebAppInstallableDelegateWebOS::Icon::Icon(Type type,
                                           const SkBitmap* bitmap,
                                           int64_t timestamp)
    : type_(type), bitmap_(bitmap) {
  std::string timestamp_string = timestamp > 0 ? std::to_string(timestamp) : "";
  switch (type) {
    case Type::REGULAR:
      // TODO: use icon_types map
      appinfo_key_ = "icon";
      file_name_ = "icon" + timestamp_string + ".png";
      break;
    case Type::MINI:
      appinfo_key_ = "miniicon";
      file_name_ = "icon-mini" + timestamp_string + ".png";
      break;
    case Type::LARGE:
      appinfo_key_ = "largeIcon";
      file_name_ = "icon-large" + timestamp_string + ".png";
      break;
    case Type::SPLASH:
      appinfo_key_ = "splashicon";
      file_name_ = "icon-splash" + timestamp_string + ".png";
      break;
  }
}

WebAppInstallableDelegateWebOS::WebAppInstallableDelegateWebOS()
    : luna_client_(InitLunaClient()), weak_ptr_factory_(this) {}

// static
std::unique_ptr<luna::Client> WebAppInstallableDelegateWebOS::InitLunaClient() {
  luna::Client::Params params;
  params.name = luna::GetServiceNameWithRandSuffix(
      luna::service_name::kChromiumInstallableManager);
  return luna::CreateClient(params);
}

bool WebAppInstallableDelegateWebOS::IsWebAppForUrlInstalled(
    const GURL& app_start_url) {
  std::string app_id = GenerateAppId(app_start_url);
  base::FilePath app_dir = GetAppDir(app_id);
  if (app_dir.empty()) {
    return true;
  }
  return base::DirectoryExists(app_dir);
}

bool WebAppInstallableDelegateWebOS::ShouldAppForURLBeUpdated(
    const GURL& app_start_url) {
  // TODO: implement and then use SAM luna API for this
  std::string app_id = GenerateAppId(app_start_url);
  return true;
}

// Put app contents in a temporary directory and ask SAM to update the app
bool WebAppInstallableDelegateWebOS::SaveArtifacts(const WebAppInfo* app_info) {
  base::FilePath app_dir;
  if (!CreateTempAppdir(app_info->id(), &app_dir)) {
    LOG(ERROR) << __func__ << "() Failed to create temporary webapp directory";
    return false;
  }
  VLOG(1) << __func__ << "() Created temporary webapp directory: " << app_dir;

  auto selected_icons = SelectIcons(app_info);
  if (!WriteIcons(app_dir, selected_icons)) {
    LOG(ERROR) << __func__ << "() Failed to write icons";
    return false;
  }

  base::Value appinfo_content(base::Value::Type::DICTIONARY);
  appinfo_content.SetStringKey("type", "web");
  appinfo_content.SetStringKey("id", app_info->id());
  appinfo_content.SetStringKey("title", app_info->title());
  appinfo_content.SetStringKey("main", app_info->start_url().spec());
  appinfo_content.SetStringKey("icon", "icon.png");
  for (const auto& icon : selected_icons) {
    appinfo_content.SetStringKey(icon.appinfo_key_, icon.file_name_);
  }
  if (app_info->background_color().has_value()) {
    appinfo_content.SetStringKey(
        "bgColor",
        BackgroundColorForWebosAppinfo(app_info->background_color().value()));
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

  CallSamAppUpdate(app_info->id(), app_dir.value());
  return true;
}

void WebAppInstallableDelegateWebOS::CallSamAppUpdate(
    const std::string& id,
    const std::string& app_dir) {
  base::Value call_params(base::Value::Type::DICTIONARY);
  call_params.SetStringKey("id", id);
  call_params.SetStringKey("appContent", app_dir);
  std::string call_params_str;
  if (!base::JSONWriter::Write(call_params, &call_params_str)) {
    LOG(ERROR) << __func__ << "() Failed to serialize luna call params";
    return;
  }

  if (luna_client_ && luna_client_->IsInitialized()) {
    std::string service_uri = pal::luna::GetServiceURI(
        pal::luna::service_uri::kApplicationManager, "dev/scanApp");
    VLOG(1) << __func__ << "() " << luna_client_->GetName() << " "
            << service_uri << " " << call_params_str;
    luna_client_->Call(
        std::move(service_uri), std::move(call_params_str),
        base::BindOnce(&WebAppInstallableDelegateWebOS::OnScanApp,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << __func__ << "() Luna client not ready";
  }
}

void WebAppInstallableDelegateWebOS::OnScanApp(
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

bool WebAppInstallableDelegateWebOS::isInfoChanged(
    const WebAppInfo* fresh_app_info) {
  VLOG(1) << "Start checking current WebOS appinfo for: "
          << fresh_app_info->start_url();
  WebAppInfo info;

  base::FilePath app_dir = GetAppDir(fresh_app_info->id());
  if (app_dir.empty()) {
    return false;
  }

  if (!base::DirectoryExists(app_dir)) {
    return true;
  }

  std::string app_info_str;
  if (!base::ReadFileToString(app_dir.Append("appinfo.json"), &app_info_str))
    return true;

  auto current_appinfo_json = base::JSONReader::Read(app_info_str);
  if (!current_appinfo_json || !current_appinfo_json->is_dict())
    return true;
  // TODO: maybe return false up to this point
  VLOG(1) << "Found current WebOS appinfo for: " << fresh_app_info->start_url();

  std::string* current_id = current_appinfo_json->FindStringKey("id");
  if (!current_id || *current_id != fresh_app_info->id())
    return true;

  std::string* current_title = current_appinfo_json->FindStringKey("title");
  if (!current_title || *current_title != fresh_app_info->title())
    return true;

  std::string* current_url = current_appinfo_json->FindStringKey("main");
  if (!current_url)
    return true;
  if (GURL(*current_url) != fresh_app_info->start_url())
    return true;

  // background_color is absl::optional<SkColor> in WebAppInfo
  std::string* background_color =
      current_appinfo_json->FindStringKey("bgColor");
  bool has_bgcolor_in_current = !!background_color;
  bool has_bgcolor_in_fresh = fresh_app_info->background_color().has_value();

  if (has_bgcolor_in_current != has_bgcolor_in_fresh)
    return true;

  if (has_bgcolor_in_fresh) {
    if (BackgroundColorForWebosAppinfo(
            fresh_app_info->background_color().value()) != *background_color)
      return true;
  }
  //  Lastly compare icons
  auto new_icons = SelectIcons(fresh_app_info);
  return AreWebosIconsDifferent(new_icons, current_appinfo_json.value(),
                                app_dir);
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

base::FilePath WebAppInstallableDelegateWebOS::GetAppDir(
    const std::string& app_id) {
  static const base::FilePath app_storage(
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ::switches::kPwaInstallPath));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kPwaInstallPath)) {
    LOG(ERROR) << __func__
               << "() Installation path for pwa should be specified.";
    return app_storage;
  }

  return app_storage.AppendASCII(app_id);
}

}  // namespace webos
}  // namespace pal
