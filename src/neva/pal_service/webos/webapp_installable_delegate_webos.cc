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

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "neva/pal_service/luna/luna_names.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// Copy of base::CreateDirectory() to set permissions other than 0700
// on directory creation
bool CreateDirectory(const base::FilePath& full_path, int mode = 0777) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (auto i = subpaths.rbegin(); i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i))
      continue;
    if (mkdir(i->value().c_str(), mode) == 0)
      continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!DirectoryExists(*i))
      return false;
  }
  return true;
}

}  // namespace

namespace pal {
namespace webos {

WebAppInstallableDelegateWebOS::Icon::Icon(Type type, const SkBitmap* bitmap)
    : type_(type), bitmap_(bitmap) {
  switch (type) {
    case Type::REGULAR:
      appinfo_key_ = "icon";
      file_name_ = "icon.png";
      break;
    case Type::MINI:
      appinfo_key_ = "miniicon";
      file_name_ = "icon-mini.png";
      break;
    case Type::LARGE:
      appinfo_key_ = "largeIcon";
      file_name_ = "icon-large.png";
      break;
    case Type::SPLASH:
      appinfo_key_ = "splashicon";
      file_name_ = "icon-splash.png";
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

bool WebAppInstallableDelegateWebOS::IsWebAppInstalled(
    const WebAppInfo* app_info) {
  return base::DirectoryExists(GetAppDir(app_info));
}

bool WebAppInstallableDelegateWebOS::SaveArtifacts(const WebAppInfo* app_info) {
  base::FilePath app_dir = GetAppDir(app_info);
  if (base::DirectoryExists(app_dir)) {
    if (!base::DeletePathRecursively(app_dir)) {
      LOG(ERROR) << __func__ << "() Failed to delete old webapp directory "
                 << app_dir;
      return false;
    }
    VLOG(1) << __func__ << "() Deleted old webapp directory " << app_dir;
  }
  if (!::CreateDirectory(app_dir)) {
    LOG(ERROR) << __func__ << "() Failed to create webapp directory "
               << app_dir;
    return false;
  }
  VLOG(1) << __func__ << "() Created directory for webapp " << app_dir;

  auto selected_icons = SelectIcons(app_info->icons());
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
        base::StringPrintf("#%06X",
                           app_info->background_color().value() & 0xFFFFFF));
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

  RegisterInstalledApp(app_info->id());
  return true;
}

void WebAppInstallableDelegateWebOS::RegisterInstalledApp(
    const std::string& id) {
  base::Value call_params(base::Value::Type::DICTIONARY);
  call_params.SetStringKey("id", id);
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
WebAppInstallableDelegateWebOS::SelectIcons(
    const std::map<WebAppInfo::SquareSizePx, SkBitmap>& icons) {
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

  selected.push_back({Icon::Type::REGULAR, &(iter_regular->second)});
  if (iter_mini != icons.cend())
    selected.push_back({Icon::Type::MINI, &(iter_mini->second)});
  if (iter_large != icons.cend())
    selected.push_back({Icon::Type::LARGE, &(iter_large->second)});
  if (iter_splash != icons.cend())
    selected.push_back({Icon::Type::SPLASH, &(iter_splash->second)});
  return selected;
}

std::string WebAppInstallableDelegateWebOS::GenerateAppId(
    const WebAppInfo* app_info) {
  std::vector<std::string> parts =
      base::SplitString(app_info->start_url().host(), ".",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string id = "pwa";
  for (auto it = parts.crbegin(); it != parts.crend(); ++it) {
    id.append(".").append(*it);
  }
  return id;
}

base::FilePath WebAppInstallableDelegateWebOS::GetAppDir(
    const WebAppInfo* app_info) {
  static const base::FilePath app_storage(
      "/media/cryptofs/apps/usr/palm/applications/");
  return app_storage.AppendASCII(app_info->id());
}

}  // namespace webos
}  // namespace pal