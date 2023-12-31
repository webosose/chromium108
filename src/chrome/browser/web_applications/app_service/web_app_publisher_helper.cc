// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <atomic>
#include <memory>
#include <ostream>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/shortcut.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chrome/common/chrome_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"  // nogncheck
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/api/file_handlers/mime_util.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

using apps::IconEffects;

namespace content {
class BrowserContext;
}

namespace web_app {

class WebAppInstallManager;

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

// Mime Type for plain text.
const char kTextPlain[] = "text/plain";

bool GetContentSettingsType(apps::PermissionType permission_type,
                            ContentSettingsType& content_setting_type) {
  switch (permission_type) {
    case apps::PermissionType::kCamera:
      content_setting_type = ContentSettingsType::MEDIASTREAM_CAMERA;
      return true;
    case apps::PermissionType::kLocation:
      content_setting_type = ContentSettingsType::GEOLOCATION;
      return true;
    case apps::PermissionType::kMicrophone:
      content_setting_type = ContentSettingsType::MEDIASTREAM_MIC;
      return true;
    case apps::PermissionType::kNotifications:
      content_setting_type = ContentSettingsType::NOTIFICATIONS;
      return true;
    case apps::PermissionType::kUnknown:
    case apps::PermissionType::kContacts:
    case apps::PermissionType::kStorage:
    case apps::PermissionType::kPrinting:
    case apps::PermissionType::kFileHandling:
      return false;
  }
}

apps::mojom::PermissionType GetPermissionType(
    ContentSettingsType content_setting_type) {
  switch (content_setting_type) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return apps::mojom::PermissionType::kCamera;
    case ContentSettingsType::GEOLOCATION:
      return apps::mojom::PermissionType::kLocation;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return apps::mojom::PermissionType::kMicrophone;
    case ContentSettingsType::NOTIFICATIONS:
      return apps::mojom::PermissionType::kNotifications;
    default:
      return apps::mojom::PermissionType::kUnknown;
  }
}

apps::mojom::InstallReason GetHighestPriorityInstallReason(
    const WebApp* web_app) {
  // TODO(crbug.com/1189949): Introduce kOem as a new WebAppManagement::Type
  // value immediately below WebAppManagement::kSystem, so that this
  // custom behavior isn't needed.
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    if (chromeos_data.oem_installed) {
      DCHECK(!web_app->IsSystemApp());
      return apps::mojom::InstallReason::kOem;
    }
  }

  switch (web_app->GetHighestPrioritySource()) {
    case WebAppManagement::kSystem:
      return apps::mojom::InstallReason::kSystem;
    case WebAppManagement::kKiosk:
      return apps::mojom::InstallReason::kKiosk;
    case WebAppManagement::kPolicy:
      return apps::mojom::InstallReason::kPolicy;
    case WebAppManagement::kSubApp:
      return apps::mojom::InstallReason::kSubApp;
    case WebAppManagement::kWebAppStore:
      return apps::mojom::InstallReason::kUser;
    case WebAppManagement::kSync:
      return apps::mojom::InstallReason::kSync;
    case WebAppManagement::kDefault:
      return apps::mojom::InstallReason::kDefault;
    case WebAppManagement::kCommandLine:
      return apps::mojom::InstallReason::kCommandLine;
  }
}

apps::mojom::InstallSource ConvertInstallSourceToMojom(
    absl::optional<webapps::WebappInstallSource> source) {
  if (!source)
    return apps::mojom::InstallSource::kUnknown;

  switch (*source) {
    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::ISOLATED_APP_DEV_INSTALL:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
    case webapps::WebappInstallSource::EXTERNAL_POLICY:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
    case webapps::WebappInstallSource::SUB_APP:
    case webapps::WebappInstallSource::CHROME_SERVICE:
    case webapps::WebappInstallSource::KIOSK:
      return apps::mojom::InstallSource::kBrowser;
    case webapps::WebappInstallSource::ARC:
      return apps::mojom::InstallSource::kPlayStore;
    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN:
    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
      return apps::mojom::InstallSource::kSystem;
    case webapps::WebappInstallSource::SYNC:
      return apps::mojom::InstallSource::kSync;
    case webapps::WebappInstallSource::COUNT:
      NOTREACHED();
      return apps::mojom::InstallSource::kUnknown;
  }
}

bool IsNoteTakingWebApp(const WebApp& web_app) {
  return web_app.note_taking_new_note_url().is_valid();
}

bool IsLockScreenCapable(const WebApp& web_app) {
  if (!base::FeatureList::IsEnabled(features::kWebLockScreenApi))
    return false;
  return web_app.lock_screen_start_url().is_valid();
}

apps::mojom::IntentFilterPtr CreateMimeTypeShareFilter(
    const std::vector<std::string>& mime_types) {
  DCHECK(!mime_types.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  action_condition_values.push_back(apps_util::MakeConditionValue(
      apps_util::kIntentActionSend, apps::mojom::PatternMatchType::kLiteral));
  auto action_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kAction, std::move(action_condition_values));
  intent_filter->conditions.push_back(std::move(action_condition));

  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  for (auto& mime_type : mime_types) {
    condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  auto mime_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kMimeType, std::move(condition_values));
  intent_filter->conditions.push_back(std::move(mime_condition));

  return intent_filter;
}

apps::IntentFilters CreateShareIntentFiltersFromShareTarget(
    const apps::ShareTarget& share_target) {
  apps::IntentFilters filters;

  if (!share_target.params.text.empty()) {
    // The share target accepts navigator.share() calls with text.
    filters.push_back(apps::ConvertMojomIntentFilterToIntentFilter(
        CreateMimeTypeShareFilter({kTextPlain})));
  }

  std::vector<std::string> content_types;
  for (const auto& files_entry : share_target.params.files) {
    for (const auto& file_type : files_entry.accept) {
      // Skip any file_type that is not a MIME type.
      if (file_type.empty() || file_type[0] == '.' ||
          std::count(file_type.begin(), file_type.end(), '/') != 1) {
        continue;
      }

      content_types.push_back(file_type);
    }
  }

  if (!content_types.empty()) {
    const std::vector<std::string> intent_actions(
        {apps_util::kIntentActionSend, apps_util::kIntentActionSendMultiple});
    filters.push_back(
        apps_util::CreateFileFilter(intent_actions, content_types, {}));
  }

  return filters;
}

apps::IntentFilters CreateIntentFiltersFromFileHandlers(
    const apps::FileHandlers& file_handlers) {
  apps::IntentFilters filters;
  for (const apps::FileHandler& handler : file_handlers) {
    std::vector<std::string> mime_types;
    std::vector<std::string> file_extensions;
    std::string action_url = handler.action.spec();
    // TODO(petermarshall): Use GetFileExtensionsFromFileHandlers /
    // GetMimeTypesFromFileHandlers?
    for (const apps::FileHandler::AcceptEntry& accept_entry : handler.accept) {
      mime_types.push_back(accept_entry.mime_type);
      for (const std::string& extension : accept_entry.file_extensions) {
        file_extensions.push_back(extension);
      }
    }
    filters.push_back(
        apps_util::CreateFileFilter({apps_util::kIntentActionView}, mime_types,
                                    file_extensions, action_url));
  }

  return filters;
}

}  // namespace

void UninstallImpl(WebAppProvider* provider,
                   const std::string& app_id,
                   apps::UninstallSource uninstall_source,
                   gfx::NativeWindow parent_window) {
  WebAppUiManagerImpl* web_app_ui_manager = WebAppUiManagerImpl::Get(provider);
  if (!web_app_ui_manager) {
    return;
  }

  WebAppDialogManager& web_app_dialog_manager =
      web_app_ui_manager->dialog_manager();
  if (web_app_dialog_manager.CanUserUninstallWebApp(app_id)) {
    webapps::WebappUninstallSource webapp_uninstall_source =
        WebAppPublisherHelper::ConvertUninstallSourceToWebAppUninstallSource(
            uninstall_source);
    web_app_dialog_manager.UninstallWebApp(app_id, webapp_uninstall_source,
                                           parent_window, base::DoNothing());
  }
}

RunOnOsLoginMode ConvertOsLoginModeToWebAppConstants(
    apps::mojom::RunOnOsLoginMode login_mode) {
  RunOnOsLoginMode web_app_constant_login_mode = RunOnOsLoginMode::kMinValue;
  switch (login_mode) {
    case apps::mojom::RunOnOsLoginMode::kWindowed:
      web_app_constant_login_mode = RunOnOsLoginMode::kWindowed;
      break;
    case apps::mojom::RunOnOsLoginMode::kNotRun:
      web_app_constant_login_mode = RunOnOsLoginMode::kNotRun;
      break;
    case apps::mojom::RunOnOsLoginMode::kUnknown:
      web_app_constant_login_mode = RunOnOsLoginMode::kNotRun;
      break;
  }
  return web_app_constant_login_mode;
}

WebAppPublisherHelper::Delegate::Delegate() = default;

WebAppPublisherHelper::Delegate::~Delegate() = default;

#if BUILDFLAG(IS_CHROMEOS)
WebAppPublisherHelper::BadgeManagerDelegate::BadgeManagerDelegate(
    const base::WeakPtr<WebAppPublisherHelper>& publisher_helper)
    : badging::BadgeManagerDelegate(publisher_helper->profile(),
                                    publisher_helper->badge_manager_),
      publisher_helper_(publisher_helper) {}

WebAppPublisherHelper::BadgeManagerDelegate::~BadgeManagerDelegate() = default;

void WebAppPublisherHelper::BadgeManagerDelegate::OnAppBadgeUpdated(
    const AppId& app_id) {
  if (!publisher_helper_) {
    return;
  }
  apps::AppPtr app =
      publisher_helper_->app_notifications_.CreateAppWithHasBadgeStatus(
          publisher_helper_->app_type(), app_id);
  DCHECK(app->has_badge.has_value());
  app->has_badge =
      publisher_helper_->ShouldShowBadge(app_id, app->has_badge.value());
  publisher_helper_->delegate_->PublishWebApp(std::move(app));
}
#endif

WebAppPublisherHelper::WebAppPublisherHelper(
    Profile* profile,
    WebAppProvider* provider,
    ash::SystemWebAppManager* swa_manager,
    Delegate* delegate,
    bool observe_media_requests)
    : profile_(profile),
      provider_(provider),
      swa_manager_(swa_manager),
      app_type_(GetWebAppType()),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);
  Init(observe_media_requests);
}

WebAppPublisherHelper::~WebAppPublisherHelper() = default;

// static
apps::AppType WebAppPublisherHelper::GetWebAppType() {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() && IsWebAppsCrosapiEnabled()) {
    return apps::AppType::kSystemWeb;
  }
#endif

  return apps::AppType::kWeb;
}

// static
bool WebAppPublisherHelper::IsSupportedWebAppPermissionType(
    ContentSettingsType permission_type) {
  return base::Contains(kSupportedPermissionTypes, permission_type);
}

// static
webapps::WebappUninstallSource
WebAppPublisherHelper::ConvertUninstallSourceToWebAppUninstallSource(
    apps::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::UninstallSource::kAppList:
      return webapps::WebappUninstallSource::kAppList;
    case apps::UninstallSource::kAppManagement:
      return webapps::WebappUninstallSource::kAppManagement;
    case apps::UninstallSource::kShelf:
      return webapps::WebappUninstallSource::kShelf;
    case apps::UninstallSource::kMigration:
      return webapps::WebappUninstallSource::kMigration;
    case apps::UninstallSource::kUnknown:
      return webapps::WebappUninstallSource::kUnknown;
  }
}

void WebAppPublisherHelper::Shutdown() {
  registrar_observation_.Reset();
  content_settings_observation_.Reset();
  is_shutting_down_ = true;
}

void WebAppPublisherHelper::SetWebAppShowInFields(const WebApp* web_app,
                                                  apps::App& app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    bool should_show_app = true;
    // TODO(b/201422755): Remove Web app specific hiding for demo mode once icon
    // load fixed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (ash::DemoSession::Get()) {
      should_show_app = ash::DemoSession::Get()->ShouldShowWebApp(
          web_app->start_url().spec());
    }
#endif
    app.show_in_launcher = chromeos_data.show_in_launcher && should_show_app;
    app.show_in_shelf = app.show_in_search =
        chromeos_data.show_in_search && should_show_app;
    app.show_in_management = chromeos_data.show_in_management;
    app.handles_intents =
        chromeos_data.handles_file_open_intents ? true : app.show_in_launcher;
    return;
  }

  // Show the app everywhere by default.
  app.show_in_launcher = true;
  app.show_in_shelf = true;
  app.show_in_search = true;
  app.show_in_management = true;
  app.handles_intents = true;
}

void WebAppPublisherHelper::SetWebAppShowInFields(apps::mojom::AppPtr& app,
                                                  const WebApp* web_app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    bool should_show_app = true;
    // TODO(b/201422755): Remove Web app specific hiding for demo mode once icon
    // load fixed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (ash::DemoSession::Get()) {
      should_show_app = ash::DemoSession::Get()->ShouldShowWebApp(
          web_app->start_url().spec());
    }
#endif
    app->show_in_launcher = chromeos_data.show_in_launcher && should_show_app
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = app->show_in_search =
        chromeos_data.show_in_search && should_show_app
            ? apps::mojom::OptionalBool::kTrue
            : apps::mojom::OptionalBool::kFalse;
    app->show_in_management = chromeos_data.show_in_management
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    app->handles_intents = chromeos_data.handles_file_open_intents
                               ? apps::mojom::OptionalBool::kTrue
                               : app->show_in_launcher;
    return;
  }

  // Show the app everywhere by default.
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_shelf = show;
  app->show_in_search = show;
  app->show_in_management = show;
  app->handles_intents = show;
}

void WebAppPublisherHelper::PopulateWebAppPermissions(
    const WebApp* web_app,
    std::vector<apps::mojom::PermissionPtr>* target) {
  const GURL& url = web_app->start_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_type = GetPermissionType(type);
    permission->value =
        apps::mojom::PermissionValue::NewTristateValue(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }

  // File handling permission.
  auto permission = apps::mojom::Permission::New();
  permission->permission_type = apps::mojom::PermissionType::kFileHandling;
  permission->value = apps::mojom::PermissionValue::NewBoolValue(
      !registrar().IsAppFileHandlerPermissionBlocked(web_app->app_id()));
  permission->is_managed = false;
  target->push_back(std::move(permission));
}

apps::Permissions WebAppPublisherHelper::CreatePermissions(
    const WebApp* web_app) {
  apps::Permissions permissions;

  const GURL& url = web_app->start_url();
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::TriState::kBlock;
        break;
      default:
        setting_val = apps::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    permissions.push_back(std::make_unique<apps::Permission>(
        apps::ConvertMojomPermissionTypeToPermissionType(
            GetPermissionType(type)),
        std::make_unique<apps::PermissionValue>(setting_val),
        /*is_managed=*/setting_info.source ==
            content_settings::SETTING_SOURCE_POLICY));
  }

  // File handling permission.
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kFileHandling,
      std::make_unique<apps::PermissionValue>(
          !registrar().IsAppFileHandlerPermissionBlocked(web_app->app_id())),
      /*is_managed=*/false));

  return permissions;
}

// static
apps::IntentFilters WebAppPublisherHelper::CreateIntentFiltersForWebApp(
    const web_app::AppId& app_id,
    const GURL& app_scope,
    const apps::ShareTarget* app_share_target,
    const apps::FileHandlers* enabled_file_handlers) {
  apps::IntentFilters filters;

  if (!app_scope.is_empty()) {
    filters.push_back(apps::ConvertMojomIntentFilterToIntentFilter(
        apps_util::CreateIntentFilterForUrlScope(app_scope)));
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          features::kMicrosoftOfficeWebAppExperiment)) {
    for (const char* scope_extension :
         ChromeOsWebAppExperiments::GetScopeExtensions(app_id)) {
      filters.push_back(apps::ConvertMojomIntentFilterToIntentFilter(
          apps_util::CreateIntentFilterForUrlScope(GURL(scope_extension))));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (app_share_target) {
    base::Extend(filters,
                 CreateShareIntentFiltersFromShareTarget(*app_share_target));
  }

  if (enabled_file_handlers) {
    base::Extend(filters,
                 CreateIntentFiltersFromFileHandlers(*enabled_file_handlers));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsProjectorEnabled() &&
      app_id == ash::kChromeUITrustedProjectorSwaAppId) {
    filters.push_back(apps::ConvertMojomIntentFilterToIntentFilter(
        apps_util::CreateIntentFilterForUrlScope(
            GURL(ash::kChromeUIUntrustedProjectorPwaUrl))));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return filters;
}

apps::AppPtr WebAppPublisherHelper::CreateWebApp(const WebApp* web_app) {
  DCHECK(!IsShuttingDown());

  apps::Readiness readiness =
      web_app->is_locally_installed()
          ? (web_app->is_uninstalling() ? apps::Readiness::kUninstalledByUser
                                        : apps::Readiness::kReady)
          : apps::Readiness::kDisabledByUser;
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(web_app->chromeos_data().has_value());
  if (web_app->chromeos_data()->is_disabled)
    readiness = apps::Readiness::kDisabledByPolicy;
#endif

  auto app = apps::AppPublisher::MakeApp(
      app_type(), web_app->app_id(), readiness,
      provider_->registrar().GetAppShortName(web_app->app_id()),
      apps::ConvertMojomInstallReasonToInstallReason(
          GetHighestPriorityInstallReason(web_app)),
      apps::ConvertMojomInstallSourceToInstallSource(
          ConvertInstallSourceToMojom(
              provider_->registrar().GetAppInstallSourceForMetrics(
                  web_app->app_id()))));

  app->description =
      provider_->registrar().GetAppDescription(web_app->app_id());
  app->additional_search_terms = web_app->additional_search_terms();

  // Web App's publisher_id the start url.
  app->publisher_id = web_app->start_url().spec();

  app->icon_key =
      std::move(*icon_key_factory_.CreateIconKey(GetIconEffects(web_app)));

  app->last_launch_time = web_app->last_launch_time();
  app->install_time = web_app->install_time();

  // For system web apps (only), the install source is |kSystem|.
  DCHECK_EQ(web_app->IsSystemApp(),
            app->install_reason == apps::InstallReason::kSystem);

  app->policy_ids = GetPolicyIds(*web_app);

  app->permissions = CreatePermissions(web_app);

  SetWebAppShowInFields(web_app, *app);

#if BUILDFLAG(IS_CHROMEOS)
  if (readiness != apps::Readiness::kReady)
    UpdateAppDisabledMode(*app);

  app->has_badge = ShouldShowBadge(
      web_app->app_id(), app_notifications_.HasNotification(web_app->app_id()));
#else
  app->has_badge = false;
#endif

  app->allow_uninstall = web_app->CanUserUninstallWebApp();
  app->paused = IsPaused(web_app->app_id());

  // Add the intent filters for PWAs.
  base::Extend(
      app->intent_filters,
      CreateIntentFiltersForWebApp(
          web_app->app_id(), registrar().GetAppScope(web_app->app_id()),
          registrar().GetAppShareTarget(web_app->app_id()),
          provider_->os_integration_manager().GetEnabledFileHandlers(
              web_app->app_id())));

  // These filters are used by the settings page to display would-be-handled
  // extensions even when the feature is not enabled for the app, whereas
  // `GetEnabledFileHandlers` above only returns the ones that currently are
  // enabled.
  const apps::FileHandlers* all_file_handlers =
      registrar().GetAppFileHandlers(web_app->app_id());
  if (all_file_handlers && !all_file_handlers->empty()) {
    std::set<std::string> extensions_set =
        apps::GetFileExtensionsFromFileHandlers(*all_file_handlers);
    app->intent_filters.push_back(apps_util::CreateFileFilter(
        {apps_util::kIntentActionPotentialFileHandler},
        /*mime_types=*/{},
        /*file_extensions=*/
        {extensions_set.begin(), extensions_set.end()}));
  }

  if (IsNoteTakingWebApp(*web_app))
    app->intent_filters.push_back(apps_util::CreateNoteTakingFilter());

  if (IsLockScreenCapable(*web_app))
    app->intent_filters.push_back(apps_util::CreateLockScreenFilter());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_app->app_id() == guest_os::kTerminalSystemAppId) {
    app->intent_filters.push_back(apps_util::CreateFileFilter(
        {apps_util::kIntentActionView},
        /*mime_types=*/
        {extensions::app_file_handler_util::kMimeTypeInodeDirectory},
        /*file_extensions=*/{}));
  }
#endif

  app->window_mode = ConvertDisplayModeToWindowMode(
      registrar().GetAppEffectiveDisplayMode(web_app->app_id()));

  const auto login_mode = registrar().GetAppRunOnOsLoginMode(web_app->app_id());
  app->run_on_os_login = apps::RunOnOsLogin(
      ConvertOsLoginMode(login_mode.value), !login_mode.user_controllable);

  for (const auto& shortcut : web_app->shortcuts_menu_item_infos()) {
    const std::string name = base::UTF16ToUTF8(shortcut.name);
    std::string shortcut_id = GenerateShortcutId();
    StoreShortcutId(shortcut_id, shortcut);
    app->shortcuts.push_back(
        std::make_unique<apps::Shortcut>(shortcut_id, name));
  }

  return app;
}

apps::mojom::AppPtr WebAppPublisherHelper::ConvertWebApp(
    const WebApp* web_app) {
  return apps::ConvertAppToMojomApp(CreateWebApp(web_app));
}

apps::AppPtr WebAppPublisherHelper::ConvertUninstalledWebApp(
    const WebApp* web_app) {
  auto app = std::make_unique<apps::App>(app_type(), web_app->app_id());
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::Readiness::kUninstalledByUser;

  SetWebAppShowInFields(web_app, *app);
  return app;
}

apps::AppPtr WebAppPublisherHelper::ConvertLaunchedWebApp(
    const WebApp* web_app) {
  auto app = std::make_unique<apps::App>(app_type(), web_app->app_id());
  app->last_launch_time = web_app->last_launch_time();
  return app;
}

void WebAppPublisherHelper::UninstallWebApp(
    const WebApp* web_app,
    apps::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  if (IsShuttingDown()) {
    return;
  }

  auto origin = url::Origin::Create(web_app->start_url());

  DCHECK(provider_);
  DCHECK(
      provider_->install_finalizer().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider_->install_finalizer().UninstallWebApp(
      web_app->app_id(), webapp_uninstall_source, base::DoNothing());
  web_app = nullptr;

  if (!clear_site_data) {
    return;
  }

  constexpr bool kClearCookies = true;
  constexpr bool kClearStorage = true;
  constexpr bool kClearCache = true;
  constexpr bool kAvoidClosingConnections = false;

  content::ClearSiteData(base::BindRepeating(
                             [](content::BrowserContext* browser_context) {
                               return browser_context;
                             },
                             base::Unretained(profile())),
                         origin, kClearCookies, kClearStorage, kClearCache,
                         kAvoidClosingConnections,
                         /*cookie_partition_key=*/absl::nullopt,
                         /*storage_key=*/absl::nullopt, base::DoNothing());
}

apps::mojom::IconKeyPtr WebAppPublisherHelper::MakeIconKey(
    const WebApp* web_app) {
  return icon_key_factory_.MakeIconKey(GetIconEffects(web_app));
}

void WebAppPublisherHelper::SetIconEffect(const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->icon_key =
      std::move(*icon_key_factory_.CreateIconKey(GetIconEffects(web_app)));
  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = true;
  delegate_->PublishWebApp(
      paused_apps_.CreateAppWithPauseStatus(app_type(), app_id, kPaused));

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (GetAppIdFromApplicationName(browser->app_name()) == app_id) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }
}

void WebAppPublisherHelper::UnpauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = false;
  delegate_->PublishWebApp(
      paused_apps_.CreateAppWithPauseStatus(app_type(), app_id, kPaused));
}

bool WebAppPublisherHelper::IsPaused(const std::string& app_id) {
  return paused_apps_.IsPaused(app_id);
}

void WebAppPublisherHelper::LoadIcon(const std::string& app_id,
                                     apps::IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     apps::IconEffects icon_effects,
                                     LoadIconCallback callback) {
  DCHECK(provider_);
  if (IsShuttingDown()) {
    return;
  }

  LoadIconFromWebApp(profile_, icon_type, size_hint_in_dip, app_id,
                     icon_effects, std::move(callback));
}

content::WebContents* WebAppPublisherHelper::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::LaunchSource launch_source,
    apps::WindowInfoPtr window_info) {
  if (IsShuttingDown()) {
    return nullptr;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return nullptr;
  }

  switch (launch_source) {
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromParentalControls:
      break;
    case apps::LaunchSource::kFromAppListGrid:
    case apps::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_MAIN,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

      break;
    case apps::LaunchSource::kFromAppListQuery:
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_SEARCH,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
      break;
    case apps::LaunchSource::kFromAppListRecommendation:
    case apps::LaunchSource::kFromShelf:
    case apps::LaunchSource::kFromFileManager:
    case apps::LaunchSource::kFromLink:
    case apps::LaunchSource::kFromOmnibox:
    case apps::LaunchSource::kFromChromeInternal:
    case apps::LaunchSource::kFromKeyboard:
    case apps::LaunchSource::kFromOtherApp:
    case apps::LaunchSource::kFromMenu:
    case apps::LaunchSource::kFromInstalledNotification:
    case apps::LaunchSource::kFromTest:
    case apps::LaunchSource::kFromArc:
    case apps::LaunchSource::kFromSharesheet:
    case apps::LaunchSource::kFromReleaseNotesNotification:
    case apps::LaunchSource::kFromFullRestore:
    case apps::LaunchSource::kFromSmartTextContextMenu:
    case apps::LaunchSource::kFromDiscoverTabNotification:
    case apps::LaunchSource::kFromManagementApi:
    case apps::LaunchSource::kFromKiosk:
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromNewTabPage:
    case apps::LaunchSource::kFromIntentUrl:
    case apps::LaunchSource::kFromOsLogin:
    case apps::LaunchSource::kFromProtocolHandler:
    case apps::LaunchSource::kFromUrlHandler:
    case apps::LaunchSource::kFromLockScreen:
      break;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));

  // The app will be launched for the currently active profile.
  return LaunchAppWithParams(std::move(params));
}

void WebAppPublisherHelper::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  if (IsShuttingDown()) {
    return;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id, event_flags, launch_source, display::kInvalidDisplayId,
      /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));
  params.launch_files = std::move(file_paths);
  LaunchAppWithFilesCheckingUserPermission(app_id, std::move(params),
                                           base::DoNothing());
}

void WebAppPublisherHelper::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::IntentPtr intent,
    apps::LaunchSource launch_source,
    apps::WindowInfoPtr window_info,
    apps::LaunchCallback callback) {
  CHECK(intent);

  if (IsShuttingDown()) {
    std::move(callback).Run(apps::LaunchResult(apps::State::FAILED));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app_id == guest_os::kTerminalSystemAppId) {
    int64_t display_id =
        window_info ? window_info->display_id : display::kInvalidDisplayId;
    guest_os::LaunchTerminalWithIntent(
        profile_, display_id, std::move(intent),
        base::BindOnce(
            [](apps::LaunchCallback callback, bool success,
               const std::string& failure_reason) {
              if (!success) {
                LOG(WARNING) << "Launch terminal failed: " << failure_reason;
              }
              std::move(callback).Run(apps::ConvertBoolToLaunchResult(success));
            },
            std::move(callback)));
    return;
  }
#endif

  LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      base::BindOnce(
          [](apps::LaunchCallback callback, apps::LaunchSource launch_source,
             const std::vector<content::WebContents*>& web_contentses) {
// TODO(crbug.com/1214763): Set ArcWebContentsData for Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
            for (content::WebContents* web_contents : web_contentses) {
              if (launch_source == apps::LaunchSource::kFromArc) {
                // Add a flag to remember this tab originated in the ARC
                // context.
                web_contents->SetUserData(
                    &arc::ArcWebContentsData::kArcTransitionFlag,
                    std::make_unique<arc::ArcWebContentsData>(web_contents));
              }
            }
#endif
            std::move(callback).Run(
                apps::ConvertBoolToLaunchResult(!web_contentses.empty()));
          },
          std::move(callback), launch_source));
}

content::WebContents* WebAppPublisherHelper::LaunchAppWithParams(
    apps::AppLaunchParams params) {
  if (IsShuttingDown()) {
    return nullptr;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Terminal SWA has custom launch code and manages its own restore data.
  if (params.app_id == guest_os::kTerminalSystemAppId) {
    guest_os::LaunchTerminalHome(profile_, params.display_id);
    return nullptr;
  }

  apps::AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.override_url,
      params.launch_source, params.display_id, params.launch_files,
      params.intent);

  // Create the FullRestoreSaveHandler instance before launching the app to
  // observe the browser window.
  full_restore::FullRestoreSaveHandler::GetInstance();
#endif

  content::WebContents* const web_contents =
      web_app_launch_manager_->OpenApplication(std::move(params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Save all launch information for system web apps, because the browser
  // session restore can't restore system web apps.
  int session_id = apps::GetSessionIdForRestoreFromWebContents(web_contents);
  if (SessionID::IsValidValue(session_id)) {
    const WebApp* web_app = GetWebApp(params_for_restore.app_id);
    const bool is_system_web_app = web_app && web_app->IsSystemApp();
    if (is_system_web_app) {
      std::unique_ptr<app_restore::AppLaunchInfo> launch_info =
          std::make_unique<app_restore::AppLaunchInfo>(
              params_for_restore.app_id, session_id,
              params_for_restore.container, params_for_restore.disposition,
              params_for_restore.display_id,
              std::move(params_for_restore.launch_files),
              std::move(params_for_restore.intent));

      // TODO(crbug.com/1368285): Determine whether override URL can be restored
      // for all SWAs.
      DCHECK(swa_manager_);
      auto system_app_type =
          swa_manager_->GetSystemAppTypeForAppId(params_for_restore.app_id);
      if (system_app_type.has_value()) {
        auto* system_app = swa_manager_->GetSystemApp(*system_app_type);
        DCHECK(system_app);
        if (system_app->ShouldRestoreOverrideUrl())
          launch_info->override_url = params_for_restore.override_url;
      }

      full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                      std::move(launch_info));
    }
  }
#endif

  return web_contents;
}

void WebAppPublisherHelper::SetPermission(const std::string& app_id,
                                          apps::PermissionPtr permission) {
  if (IsShuttingDown()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  if (permission->permission_type == apps::PermissionType::kFileHandling) {
    if (permission->value &&
        absl::holds_alternative<bool>(permission->value->value)) {
      PersistFileHandlersUserChoice(profile_, app_id,
                                    absl::get<bool>(permission->value->value),
                                    base::DoNothing());
    }
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->start_url();

  ContentSettingsType permission_type;

  if (!GetContentSettingsType(permission->permission_type, permission_type)) {
    return;
  }

  DCHECK(permission->value);
  DCHECK(absl::holds_alternative<apps::TriState>(permission->value->value));
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (absl::get<apps::TriState>(permission->value->value)) {
    case apps::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, permission_value);
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPublisherHelper::StopApp(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  if (!IsWebAppsCrosapiEnabled()) {
    return;
  }

  apps::BrowserAppInstanceTracker* instance_tracker =
      apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->BrowserAppInstanceTracker();

  instance_tracker->StopInstancesOfApp(app_id);
}
#endif

void WebAppPublisherHelper::OpenNativeSettings(const std::string& app_id) {
  if (IsShuttingDown()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile(), web_app->start_url());
}

apps::WindowMode WebAppPublisherHelper::GetWindowMode(
    const std::string& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app)
    return apps::WindowMode::kUnknown;

  auto display_mode = registrar().GetAppEffectiveDisplayMode(web_app->app_id());
  return ConvertDisplayModeToWindowMode(display_mode);
}

void WebAppPublisherHelper::SetWindowMode(const std::string& app_id,
                                          apps::WindowMode window_mode) {
  auto user_display_mode = UserDisplayMode::kStandalone;
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      user_display_mode = UserDisplayMode::kBrowser;
      break;
    case apps::WindowMode::kUnknown:
    case apps::WindowMode::kWindow:
      user_display_mode = UserDisplayMode::kStandalone;
      break;
    case apps::WindowMode::kTabbedWindow:
      user_display_mode = UserDisplayMode::kTabbed;
      break;
  }
  provider_->sync_bridge().SetAppUserDisplayMode(app_id, user_display_mode,
                                                 /*is_user_action=*/true);
}

void WebAppPublisherHelper::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          &provider_->registrar(), &provider_->os_integration_manager(),
          &provider_->sync_bridge(), app_id,
          ConvertOsLoginModeToWebAppConstants(run_on_os_login_mode),
          base::DoNothing()));
}

apps::WindowMode WebAppPublisherHelper::ConvertDisplayModeToWindowMode(
    blink::mojom::DisplayMode display_mode) {
  switch (display_mode) {
    case blink::mojom::DisplayMode::kUndefined:
      return apps::WindowMode::kUnknown;
    case blink::mojom::DisplayMode::kBrowser:
      return apps::WindowMode::kBrowser;
    case blink::mojom::DisplayMode::kTabbed:
      return apps::WindowMode::kTabbedWindow;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
      return apps::WindowMode::kWindow;
  }
}

void WebAppPublisherHelper::PublishWindowModeUpdate(
    const std::string& app_id,
    blink::mojom::DisplayMode display_mode) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->window_mode = ConvertDisplayModeToWindowMode(display_mode);
  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::PublishRunOnOsLoginModeUpdate(
    const std::string& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto app = std::make_unique<apps::App>(app_type(), app_id);
  const auto login_mode = registrar().GetAppRunOnOsLoginMode(app_id);
  app->run_on_os_login = apps::RunOnOsLogin(
      ConvertOsLoginMode(run_on_os_login_mode), !login_mode.user_controllable);
  delegate_->PublishWebApp(std::move(app));
}

std::string WebAppPublisherHelper::GenerateShortcutId() {
  return base::NumberToString(shortcut_id_generator_.GenerateNextId().value());
}

void WebAppPublisherHelper::StoreShortcutId(
    const std::string& shortcut_id,
    const WebAppShortcutsMenuItemInfo& menu_item_info) {
  shortcut_id_map_.emplace(shortcut_id, std::move(menu_item_info));
}

content::WebContents* WebAppPublisherHelper::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return nullptr;
  }

  DisplayMode display_mode = registrar().GetAppEffectiveDisplayMode(app_id);

  apps::AppLaunchParams params(
      app_id, ConvertDisplayModeToAppLaunchContainer(display_mode),
      WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromMenu,
      display_id);

  auto menu_item = shortcut_id_map_.find(shortcut_id);
  if (menu_item != shortcut_id_map_.end()) {
    params.override_url = menu_item->second.url;
  }

  return LaunchAppWithParams(std::move(params));
}

WebAppRegistrar& WebAppPublisherHelper::registrar() const {
  return provider_->registrar();
}

WebAppInstallManager& WebAppPublisherHelper::install_manager() const {
  return provider_->install_manager();
}

bool WebAppPublisherHelper::IsShuttingDown() const {
  return is_shutting_down_;
}

void WebAppPublisherHelper::OnWebAppFileHandlerApprovalStateChanged(
    const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppInstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppInstalledWithOsHooks(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app) {
    delegate_->PublishWebApp(CreateWebApp(web_app));
  }
}

void WebAppPublisherHelper::OnWebAppWillBeUninstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  paused_apps_.MaybeRemoveApp(app_id);

#if BUILDFLAG(IS_CHROMEOS)
  app_notifications_.RemoveNotificationsForApp(app_id);

  auto result = media_requests_.RemoveRequests(app_id);
  delegate_->ModifyWebAppCapabilityAccess(app_id, result.camera,
                                          result.microphone);
#endif

  delegate_->PublishWebApp(ConvertUninstalledWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppPublisherHelper::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppPublisherHelper::OnWebAppLocallyInstalledStateChanged(
    const AppId& app_id,
    bool is_locally_installed) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  delegate_->PublishWebApp(CreateWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  delegate_->PublishWebApp(ConvertLaunchedWebApp(web_app));
}

void WebAppPublisherHelper::OnWebAppUserDisplayModeChanged(
    const AppId& app_id,
    UserDisplayMode user_display_mode) {
  PublishWindowModeUpdate(app_id,
                          registrar().GetAppEffectiveDisplayMode(app_id));
}

void WebAppPublisherHelper::OnWebAppRunOnOsLoginModeChanged(
    const AppId& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  PublishRunOnOsLoginModeUpdate(app_id, run_on_os_login_mode);
}

#if BUILDFLAG(IS_CHROMEOS)
// If is_disabled is set, the app backed by |app_id| is published with readiness
// kDisabledByPolicy, otherwise it's published with readiness kReady.
void WebAppPublisherHelper::OnWebAppDisabledStateChanged(const AppId& app_id,
                                                         bool is_disabled) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  DCHECK_EQ(is_disabled, web_app->chromeos_data()->is_disabled);
  apps::AppPtr app = CreateWebApp(web_app);
  app->icon_key =
      std::move(*icon_key_factory_.CreateIconKey(GetIconEffects(web_app)));
  ;

  // If the disable mode is hidden, update the visibility of the new disabled
  // app.
  if (is_disabled && provider_->policy_manager().IsDisabledAppsModeHidden()) {
    UpdateAppDisabledMode(*app);
  }

  delegate_->PublishWebApp(std::move(app));
}

void WebAppPublisherHelper::OnWebAppsDisabledModeChanged() {
  std::vector<apps::AppPtr> apps;
  std::vector<AppId> app_ids = registrar().GetAppIds();
  for (const auto& id : app_ids) {
    // We only update visibility of disabled apps in this method. When enabling
    // previously disabled app, OnWebAppDisabledStateChanged() method will be
    // called and this method will update visibility and readiness of the newly
    // enabled app.
    if (provider_->policy_manager().IsWebAppInDisabledList(id)) {
      const WebApp* web_app = GetWebApp(id);
      if (!web_app) {
        continue;
      }
      auto app = std::make_unique<apps::App>(app_type(), web_app->app_id());
      UpdateAppDisabledMode(*app);
      apps.push_back(std::move(app));
    }
  }
  delegate_->PublishWebApps(std::move(apps));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPublisherHelper::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  if (notification.notifier_id().type !=
      message_center::NotifierType::WEB_PAGE) {
    return;
  }
  MaybeAddWebPageNotifications(notification, metadata);
}

void WebAppPublisherHelper::OnNotificationClosed(
    const std::string& notification_id) {
  auto app_ids = app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    auto app =
        app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id);
    DCHECK(app->has_badge.has_value());
    app->has_badge = ShouldShowBadge(app_id, app->has_badge.value());
    delegate_->PublishWebApp(std::move(app));
  }
}

void WebAppPublisherHelper::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  DCHECK(notification_display_service_.IsObservingSource(service));
  notification_display_service_.Reset();
}

void WebAppPublisherHelper::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
    return;
  }

  absl::optional<AppId> app_id =
      FindInstalledAppWithUrlInScope(profile(), web_contents->GetVisibleURL(),
                                     /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  if (media_requests_.IsNewRequest(app_id.value(), web_contents, state)) {
    content::WebContentsUserData<
        apps::AppWebContentsData>::CreateForWebContents(web_contents, this);
  }

  auto result = media_requests_.UpdateRequests(app_id.value(), web_contents,
                                               stream_type, state);
  delegate_->ModifyWebAppCapabilityAccess(app_id.value(), result.camera,
                                          result.microphone);
}

void WebAppPublisherHelper::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  absl::optional<AppId> app_id = FindInstalledAppWithUrlInScope(
      profile(), web_contents->GetLastCommittedURL(),
      /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  auto result =
      media_requests_.OnWebContentsDestroyed(app_id.value(), web_contents);
  delegate_->ModifyWebAppCapabilityAccess(app_id.value(), result.camera,
                                          result.microphone);
}
#endif

void WebAppPublisherHelper::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK(!IsShuttingDown());
  // If content_type is not one of the supported permissions, do nothing.
  if (!content_type_set.ContainsAllTypes() &&
      !IsSupportedWebAppPermissionType(content_type_set.GetType())) {
    return;
  }

  for (const WebApp& web_app : registrar().GetApps()) {
    if (primary_pattern.Matches(web_app.start_url())) {
      auto app = std::make_unique<apps::App>(app_type(), web_app.app_id());
      app->permissions = CreatePermissions(&web_app);
      delegate_->PublishWebApp(std::move(app));
    }
  }
}

void WebAppPublisherHelper::OnWebAppSettingsPolicyChanged() {
  DCHECK(!IsShuttingDown());
  // TODO(crbug.com/1293961): when more features are added to policy manager, we
  // need to remove per-feature updates in favor of a full refresh, as each
  // feature multiplicatively increases the complexity of this operation.
  for (const WebApp& web_app : registrar().GetApps()) {
    const auto login_mode =
        registrar().GetAppRunOnOsLoginMode(web_app.app_id());

    PublishRunOnOsLoginModeUpdate(web_app.app_id(), login_mode.value);
  }
}

void WebAppPublisherHelper::Init(bool observe_media_requests) {
  // Allow for web app migration tests.
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  DCHECK(provider_);
  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppPublisherHelper::ObserveWebAppSubsystems,
                                weak_ptr_factory_.GetWeakPtr()));

  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile_));

#if BUILDFLAG(IS_CHROMEOS)
  notification_display_service_.Observe(
      NotificationDisplayServiceFactory::GetForProfile(profile()));

  badge_manager_ = badging::BadgeManagerFactory::GetForProfile(profile());
  // badge_manager_ is nullptr in guest and incognito profiles.
  if (badge_manager_) {
    badge_manager_->SetDelegate(
        std::make_unique<WebAppPublisherHelper::BadgeManagerDelegate>(
            weak_ptr_factory_.GetWeakPtr()));
  }
#endif

  web_app_launch_manager_ = std::make_unique<WebAppLaunchManager>(profile_);

#if BUILDFLAG(IS_CHROMEOS)
  if (observe_media_requests) {
    media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());
  }
#endif
}

void WebAppPublisherHelper::ObserveWebAppSubsystems() {
  install_manager_observation_.Observe(&install_manager());
  registrar_observation_.Observe(&registrar());
}

IconEffects WebAppPublisherHelper::GetIconEffects(const WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kRoundCorners;
  if (!web_app->is_locally_installed()) {
    icon_effects |= IconEffects::kBlocked;
  }

#if BUILDFLAG(IS_CHROMEOS)
  icon_effects |= web_app->is_generated_icon() ? IconEffects::kCrOsStandardMask
                                               : IconEffects::kCrOsStandardIcon;
#endif

  if (IsPaused(web_app->app_id())) {
    icon_effects |= IconEffects::kPaused;
  }

  bool is_disabled = false;
  if (web_app->chromeos_data().has_value()) {
    is_disabled = web_app->chromeos_data()->is_disabled;
  }
  if (is_disabled) {
    icon_effects |= IconEffects::kBlocked;
  }

  return icon_effects;
}

const WebApp* WebAppPublisherHelper::GetWebApp(const AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

void WebAppPublisherHelper::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::IntentPtr intent,
    apps::LaunchSource launch_source,
    int64_t display_id,
    base::OnceCallback<void(const std::vector<content::WebContents*>&)>
        callback) {
  bool is_file_handling_launch =
      intent && !intent->files.empty() && !intent->IsShareIntent();
  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, launch_source, display_id,
      ConvertDisplayModeToAppLaunchContainer(
          registrar().GetAppEffectiveDisplayMode(app_id)),
      std::move(intent), profile_);
  if (is_file_handling_launch) {
    LaunchAppWithFilesCheckingUserPermission(app_id, std::move(params),
                                             std::move(callback));
    return;
  }

  std::move(callback).Run({LaunchAppWithParams(std::move(params))});
}

std::vector<std::string> WebAppPublisherHelper::GetPolicyIds(
    const WebApp& web_app) const {
  const auto& app_id = web_app.app_id();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // File Manager SWA uses File Manager Extension's ID for policy.
  if (app_id == file_manager::kFileManagerSwaAppId) {
    return {file_manager::kFileManagerAppId};
  }
#endif

  if (!registrar().HasExternalAppWithInstallSource(
          app_id, ExternalInstallSource::kExternalPolicy)) {
    return {};
  }

  base::flat_map<AppId, base::flat_set<GURL>> installed_apps =
      registrar().GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  if (auto it = installed_apps.find(app_id); it != installed_apps.end()) {
    const auto& install_urls = it->second;
    DCHECK(!install_urls.empty());

    std::vector<std::string> policy_ids;
    base::ranges::transform(install_urls, std::back_inserter(policy_ids),
                            &GURL::spec);
    return policy_ids;
  }

  return {};
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPublisherHelper::UpdateAppDisabledMode(apps::App& app) {
  if (provider_->policy_manager().IsDisabledAppsModeHidden()) {
    app.show_in_launcher = false;
    app.show_in_search = false;
    app.show_in_shelf = false;
    return;
  }
  app.show_in_launcher = true;
  app.show_in_search = true;
  app.show_in_shelf = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(swa_manager_);
  auto system_app_type = swa_manager_->GetSystemAppTypeForAppId(app.app_id);
  if (system_app_type.has_value()) {
    auto* system_app = swa_manager_->GetSystemApp(*system_app_type);
    DCHECK(system_app);
    app.show_in_launcher = system_app->ShouldShowInLauncher();
    app.show_in_search = system_app->ShouldShowInSearch();
    app.show_in_shelf = app.show_in_search;
  }
#endif
}

void WebAppPublisherHelper::UpdateAppDisabledMode(apps::mojom::AppPtr& app) {
  if (provider_->policy_manager().IsDisabledAppsModeHidden()) {
    app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    app->show_in_search = apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
    return;
  }
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(swa_manager_);
  auto system_app_type = swa_manager_->GetSystemAppTypeForAppId(app->app_id);
  if (system_app_type.has_value()) {
    auto* system_app = swa_manager_->GetSystemApp(*system_app_type);
    DCHECK(system_app);
    app->show_in_launcher = system_app->ShouldShowInLauncher()
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_search = system_app->ShouldShowInSearch()
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = app->show_in_search;
  }
#endif
}

bool WebAppPublisherHelper::MaybeAddNotification(
    const std::string& app_id,
    const std::string& notification_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  auto app = app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id);
  DCHECK(app->has_badge.has_value());
  app->has_badge = ShouldShowBadge(app_id, app->has_badge.value());
  delegate_->PublishWebApp(std::move(app));
  return true;
}

void WebAppPublisherHelper::MaybeAddWebPageNotifications(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  const PersistentNotificationMetadata* persistent_metadata =
      PersistentNotificationMetadata::From(metadata);

  const NonPersistentNotificationMetadata* non_persistent_metadata =
      NonPersistentNotificationMetadata::From(metadata);

  if (persistent_metadata) {
    // For persistent notifications, find the web app with the SW scope url.
    absl::optional<AppId> app_id = FindInstalledAppWithUrlInScope(
        profile(), persistent_metadata->service_worker_scope,
        /*window_only=*/false);
    if (app_id.has_value()) {
      MaybeAddNotification(app_id.value(), notification.id());
    }
  } else {
    // For non-persistent notifications, find all web apps that are installed
    // under the origin url.

    const GURL& url = non_persistent_metadata &&
                              !non_persistent_metadata->document_url.is_empty()
                          ? non_persistent_metadata->document_url
                          : notification.origin_url();

    auto app_ids = registrar().FindAppsInScope(url);
    int count = 0;
    for (const auto& app_id : app_ids) {
      if (MaybeAddNotification(app_id, notification.id())) {
        ++count;
      }
    }
    apps::RecordAppsPerNotification(count);
  }
}

bool WebAppPublisherHelper::ShouldShowBadge(const std::string& app_id,
                                            bool has_notification) {
  // We show a badge if either the Web Badging API recently has a badge set, or
  // the Badging API has not been recently used by the app and a notification is
  // showing.
  if (!badge_manager_ || !badge_manager_->HasRecentApiUsage(app_id))
    return has_notification;

  return badge_manager_->GetBadgeValue(app_id).has_value();
}
#endif

void WebAppPublisherHelper::LaunchAppWithFilesCheckingUserPermission(
    const std::string& app_id,
    apps::AppLaunchParams params,
    base::OnceCallback<void(const std::vector<content::WebContents*>&)>
        callback) {
  DCHECK(
      provider_->os_integration_manager().IsFileHandlingAPIAvailable(app_id));

  std::vector<base::FilePath> file_paths = params.launch_files;
  auto launch_callback =
      base::BindOnce(&WebAppPublisherHelper::OnFileHandlerDialogCompleted,
                     weak_ptr_factory_.GetWeakPtr(), app_id, std::move(params),
                     std::move(callback));

  switch (provider_->registrar().GetAppFileHandlerApprovalState(app_id)) {
    case ApiApprovalState::kRequiresPrompt:
      chrome::ShowWebAppFileLaunchDialog(file_paths, profile(), app_id,
                                         std::move(launch_callback));
      break;
    case ApiApprovalState::kAllowed:
      std::move(launch_callback)
          .Run(/*allowed=*/true, /*remember_user_choice=*/false);
      break;
    case ApiApprovalState::kDisallowed:
      // We shouldn't have gotten this far (i.e. "open with" should not have
      // been selectable) if file handling was already disallowed for the app.
      NOTREACHED();
      std::move(launch_callback)
          .Run(/*allowed=*/false, /*remember_user_choice=*/false);
      break;
  }
}

void WebAppPublisherHelper::OnFileHandlerDialogCompleted(
    std::string app_id,
    apps::AppLaunchParams params,
    base::OnceCallback<void(const std::vector<content::WebContents*>&)>
        callback,
    bool allowed,
    bool remember_user_choice) {
  if (remember_user_choice) {
    PersistFileHandlersUserChoice(profile(), app_id, allowed,
                                  base::DoNothing());
  }

  if (!allowed) {
    std::move(callback).Run({});
    return;
  }

  // System web apps behave differently than when launching a normal PWA with
  // the File Handling API. Per the web spec, PWAs require that the extension
  // matches what's specified in the manifest. System apps rely on MIME type
  // sniffing to work even when the extensions don't match. For this reason,
  // `GetMatchingFileHandlerUrls` and therefore multilaunch won't work for
  // system apps.
  const WebApp* web_app = GetWebApp(params.app_id);
  bool can_multilaunch = !(web_app && web_app->IsSystemApp());
  std::vector<content::WebContents*> web_contentses;
  if (can_multilaunch) {
    WebAppFileHandlerManager::LaunchInfos file_launch_infos =
        provider_->os_integration_manager()
            .file_handler_manager()
            .GetMatchingFileHandlerUrls(app_id, params.launch_files);
    for (const auto& [url, files] : file_launch_infos) {
      apps::AppLaunchParams params_for_file_launch(
          app_id, params.container, params.disposition, params.launch_source,
          params.display_id, files, nullptr);
      params_for_file_launch.override_url = url;
      web_contentses.push_back(
          LaunchAppWithParams(std::move(params_for_file_launch)));
    }
  } else {
    apps::AppLaunchParams params_for_file_launch(
        app_id, params.container, params.disposition, params.launch_source,
        params.display_id, params.launch_files, params.intent);
    // For system web apps, the URL is calculated by the file browser and passed
    // in the intent.
    // TODO(crbug.com/1264164): remove this check. It's only here to support
    // tests that haven't been updated.
    if (params.intent) {
      params_for_file_launch.override_url = GURL(*params.intent->activity_name);
    }
    web_contentses.push_back(
        LaunchAppWithParams(std::move(params_for_file_launch)));
  }
  std::move(callback).Run(web_contentses);
}

}  // namespace web_app
