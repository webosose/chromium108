// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"  // nogncheck
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#endif

using apps::IconEffects;

namespace web_app {

namespace {

bool ShouldObserveMediaRequests() {
  return true;
}

}  // namespace

WebApps::WebApps(apps::AppServiceProxy* proxy)
    : apps::AppPublisher(proxy),
      profile_(proxy->profile()),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      instance_registry_(&proxy->InstanceRegistry()),
      publisher_helper_(
          profile_,
          provider_,
          ash::SystemWebAppManager::GetForLocalAppsUnchecked(profile_),
          this,
          ShouldObserveMediaRequests())
#else
      publisher_helper_(profile_,
                        provider_,
                        /*swa_manager=*/nullptr,
                        this,
                        ShouldObserveMediaRequests())
#endif
{
  Initialize(proxy->AppService());
}

WebApps::~WebApps() = default;

void WebApps::Shutdown() {
  if (provider_) {
    publisher_helper().Shutdown();
  }
}

const WebApp* WebApps::GetWebApp(const AppId& app_id) const {
  DCHECK(provider_);
  return provider_->registrar().GetAppById(app_id);
}

void WebApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  DCHECK(provider_);

  PublisherBase::Initialize(app_service,
                            apps::ConvertAppTypeToMojomAppType(app_type()));

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::InitWebApps, AsWeakPtr()));
}

void WebApps::LoadIcon(const std::string& app_id,
                       const apps::IconKey& icon_key,
                       apps::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       apps::LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, icon_type, size_hint_in_dip,
                              static_cast<IconEffects>(icon_key.icon_effects),
                              std::move(callback));
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::LaunchSource launch_source,
                     apps::WindowInfoPtr window_info) {
  publisher_helper().Launch(app_id, event_flags, launch_source,
                            std::move(window_info));
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::LaunchSource launch_source,
                                 std::vector<base::FilePath> file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, event_flags, launch_source,
                                        std::move(file_paths));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::IntentPtr intent,
                                  apps::LaunchSource launch_source,
                                  apps::WindowInfoPtr window_info,
                                  apps::LaunchCallback callback) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         launch_source, std::move(window_info),
                                         std::move(callback));
}

void WebApps::LaunchAppWithParams(apps::AppLaunchParams&& params,
                                  apps::LaunchCallback callback) {
  publisher_helper().LaunchAppWithParams(std::move(params));
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(apps::LaunchResult());
}

void WebApps::LaunchShortcut(const std::string& app_id,
                             const std::string& shortcut_id,
                             int64_t display_id) {
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id);
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::Uninstall(const std::string& app_id,
                        apps::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void WebApps::GetMenuModel(const std::string& app_id,
                           apps::MenuType menu_type,
                           int64_t display_id,
                           base::OnceCallback<void(apps::MenuItems)> callback) {
  const auto* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  apps::MenuItems menu_items;
  if (web_app->IsSystemApp()) {
    DCHECK(web_app->client_data().system_web_app_data.has_value());
    ash::SystemWebAppType swa_type =
        web_app->client_data().system_web_app_data->system_app_type;

    auto* system_app =
        ash::SystemWebAppManager::Get(profile())->GetSystemApp(swa_type);
    if (system_app && system_app->ShouldShowNewWindowMenuOption()) {
      apps::AddCommandItem(ash::LAUNCH_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, menu_items);
    }
  } else {
    apps::CreateOpenNewSubmenu(
        publisher_helper().GetWindowMode(app_id) == apps::WindowMode::kBrowser
            ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
            : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
        menu_items);
  }

  if (app_id == guest_os::kTerminalSystemAppId) {
    guest_os::AddTerminalMenuItems(profile_, menu_items);
  }

  if (menu_type == apps::MenuType::kShelf &&
      instance_registry_->ContainsAppId(app_id)) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         menu_items);
  }

  if (web_app->CanUserUninstallWebApp()) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         menu_items);
  }

  if (!web_app->IsSystemApp()) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         menu_items);
  }

  if (app_id == guest_os::kTerminalSystemAppId) {
    guest_os::AddTerminalMenuShortcuts(profile_, ash::LAUNCH_APP_SHORTCUT_FIRST,
                                       std::move(menu_items),
                                       std::move(callback));
  } else {
    GetAppShortcutMenuModel(app_id, std::move(menu_items), std::move(callback));
  }
}
#endif

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}

void WebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::StartPublishingWebApps, AsWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().Launch(
      app_id, event_flags,
      apps::ConvertMojomLaunchSourceToLaunchSource(launch_source),
      apps::ConvertMojomWindowInfoToWindowInfo(window_info));
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::FilePathsPtr file_paths) {
  publisher_helper().LaunchAppWithFiles(
      app_id, event_flags,
      apps::ConvertMojomLaunchSourceToLaunchSource(launch_source),
      apps::ConvertMojomFilePathsToFilePaths(std::move(file_paths)));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  apps::mojom::WindowInfoPtr window_info,
                                  LaunchAppWithIntentCallback callback) {
  publisher_helper().LaunchAppWithIntent(
      app_id, event_flags, apps::ConvertMojomIntentToIntent(intent),
      apps::ConvertMojomLaunchSourceToLaunchSource(launch_source),
      apps::ConvertMojomWindowInfoToWindowInfo(window_info),
      base::BindOnce(
          [](LaunchAppWithIntentCallback callback,
             apps::LaunchResult&& result) {
            std::move(callback).Run(apps::ConvertLaunchResultToBool(result));
          },
          std::move(callback)));
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(
      app_id, apps::ConvertMojomPermissionToPermission(permission));
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::mojom::WindowMode window_mode) {
  publisher_helper().SetWindowMode(
      app_id, apps::ConvertMojomWindowModeToWindowMode(window_mode));
}

void WebApps::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  publisher_helper().SetRunOnOsLoginMode(app_id, run_on_os_login_mode);
}

void WebApps::PublishWebApps(std::vector<apps::AppPtr> apps) {
  if (!is_ready_) {
    return;
  }

  if (apps.empty()) {
    return;
  }

  std::vector<apps::mojom::AppPtr> mojom_apps;
  mojom_apps.reserve(apps.size());
  for (const apps::AppPtr& app : apps) {
    mojom_apps.push_back(apps::ConvertAppToMojomApp(app));
  }

  apps::AppPublisher::Publish(std::move(apps), app_type(),
                              /*should_notify_initialized=*/false);

  const bool should_notify_initialized = false;
  if (subscribers_.size() == 1) {
    auto& subscriber = *subscribers_.begin();
    subscriber->OnApps(std::move(mojom_apps),
                       apps::ConvertAppTypeToMojomAppType(app_type()),
                       should_notify_initialized);
    return;
  }
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> cloned_apps;
    cloned_apps.reserve(mojom_apps.size());
    for (const auto& app : mojom_apps)
      cloned_apps.push_back(app.Clone());
    subscriber->OnApps(std::move(cloned_apps),
                       apps::ConvertAppTypeToMojomAppType(app_type()),
                       should_notify_initialized);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const WebApp* web_app = GetWebApp(ash::kChromeUITrustedProjectorSwaAppId);
  if (web_app) {
    proxy()->SetSupportedLinksPreference(
        ash::kChromeUITrustedProjectorSwaAppId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebApps::PublishWebApp(apps::AppPtr app) {
  if (!is_ready_) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_projector = app->app_id == ash::kChromeUITrustedProjectorSwaAppId;
#endif

  auto mojom_app = apps::ConvertAppToMojomApp(app);
  apps::AppPublisher::Publish(std::move(app));
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_projector) {
    // After OOBE, PublishWebApps() above could execute before the Projector app
    // has been registered. Since we need to call SetSupportedLinksPreference()
    // after the intent filter has been registered, we need this call for the
    // OOBE case.
    proxy()->SetSupportedLinksPreference(
        ash::kChromeUITrustedProjectorSwaAppId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void WebApps::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (base::FeatureList::IsEnabled(
          apps::kAppServiceCapabilityAccessWithoutMojom)) {
    apps::AppPublisher::ModifyCapabilityAccess(
        app_id, std::move(accessing_camera), std::move(accessing_microphone));
    return;
  }

  PublisherBase::ModifyCapabilityAccess(subscribers_, app_id,
                                        std::move(accessing_camera),
                                        std::move(accessing_microphone));
}

std::vector<apps::AppPtr> WebApps::CreateWebApps() {
  DCHECK(provider_);

  std::vector<apps::AppPtr> apps;
  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    apps.push_back(publisher_helper().CreateWebApp(&web_app));
  }
  return apps;
}

void WebApps::ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out) {
  DCHECK(provider_);
  if (publisher_helper().IsShuttingDown()) {
    return;
  }

  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    apps_out->push_back(publisher_helper().ConvertWebApp(&web_app));
  }
}

void WebApps::InitWebApps() {
  is_ready_ = true;

  RegisterPublisher(app_type());

  std::vector<apps::AppPtr> apps = CreateWebApps();
  apps::AppPublisher::Publish(std::move(apps), app_type(),
                              /*should_notify_initialized=*/true);
}

void WebApps::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  is_ready_ = true;

  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(&apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps),
                     apps::ConvertAppTypeToMojomAppType(app_type()),
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::Uninstall(const std::string& app_id,
                        apps::mojom::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  Uninstall(
      app_id,
      apps::ConvertMojomUninstallSourceToUninstallSource(uninstall_source),
      clear_site_data, report_abuse);
}

void WebApps::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebApps::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebApps::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

void WebApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  GetMenuModel(app_id, apps::ConvertMojomMenuTypeToMenuType(menu_type),
               display_id,
               apps::MenuItemsToMojomMenuItemsCallback(std::move(callback)));
}

void WebApps::GetAppShortcutMenuModel(
    const std::string& app_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (!web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebApps::OnShortcutsMenuIconsRead,
                               base::AsWeakPtr<WebApps>(this), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebApps::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::MenuItems menu_items,
    base::OnceCallback<void(apps::MenuItems)> callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::MenuItems());
    return;
  }

  apps::AddSeparator(ui::DOUBLE_SEPARATOR, menu_items);

  size_t menu_item_index = 0;

  for (const WebAppShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    if (menu_item_index != 0) {
      apps::AddSeparator(ui::PADDED_SEPARATOR, menu_items);
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      IconEffects icon_effects = IconEffects::kNone;

      // We apply masking to each shortcut icon, regardless if the purpose is
      // |MASKABLE| or |ANY|.
      icon_effects = apps::kCrOsStandardBackground | apps::kCrOsStandardMask;

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);

    apps::AddShortcutCommandItem(command_id, shortcut_id, label, icon,
                                 menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  if (app_id == guest_os::kTerminalSystemAppId) {
    if (guest_os::ExecuteTerminalMenuShortcutCommand(profile_, shortcut_id,
                                                     display_id)) {
      return;
    }
  }
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id);
}

#endif

}  // namespace web_app
