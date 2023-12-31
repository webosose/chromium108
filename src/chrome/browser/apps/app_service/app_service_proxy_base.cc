// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/extend.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/common/constants.h"
#include "ui/display/types/display_constants.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/app_id.h"
#endif

namespace apps {

namespace {

// Utility struct used in GetAppsForIntent.
struct IndexAndGeneric {
  size_t index;
  bool is_generic;
};

std::string GetActivityLabel(const IntentFilterPtr& filter,
                             const AppUpdate& update) {
  if (filter->activity_label.has_value() && !filter->activity_label->empty()) {
    return filter->activity_label.value();
  } else {
    return update.Name();
  }
}

}  // anonymous namespace

AppServiceProxyBase::InnerIconLoader::InnerIconLoader(AppServiceProxyBase* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

absl::optional<IconKey> AppServiceProxyBase::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  absl::optional<IconKey> icon_key;
  host_->app_registry_cache_.ForOneApp(
      app_id,
      [&icon_key](const AppUpdate& update) { icon_key = update.IconKey(); });
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyBase::InnerIconLoader::LoadIconFromIconKey(
    AppType app_type,
    const std::string& app_id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, icon_key, icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  auto* publisher = host_->GetPublisher(app_type);
  if (!publisher) {
    LOG(WARNING) << "No publisher for requested icon";
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  RecordIconLoadMethodMetrics(IconLoadingMethod::kViaNonMojomCall);
  publisher->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon, std::move(callback));
  return nullptr;
}

AppServiceProxyBase::AppServiceProxyBase(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {
  preferred_apps_impl_ = std::make_unique<apps::PreferredAppsImpl>(
      this, profile ? profile->GetPath() : base::FilePath());
}

AppServiceProxyBase::~AppServiceProxyBase() = default;

void AppServiceProxyBase::ReinitializeForTesting(
    Profile* profile,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  profile_ = profile;
  is_using_testing_profile_ = true;
  app_registry_cache_.ReinitializeForTesting();  // IN-TEST

  preferred_apps_impl_ = std::make_unique<apps::PreferredAppsImpl>(
      this, profile ? profile->GetPath() : base::FilePath(),
      std::move(read_completed_for_testing),
      std::move(write_completed_for_testing));

  publishers_.clear();
  Initialize();
}

bool AppServiceProxyBase::IsValidProfile() {
  if (!profile_) {
    return false;
  }

  // We only initialize the App Service for regular or guest profiles. Non-guest
  // off-the-record profiles do not get an instance.
  if (profile_->IsOffTheRecord() && !profile_->IsGuestSession()) {
    return false;
  }

  return true;
}

void AppServiceProxyBase::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  browser_app_launcher_ = std::make_unique<apps::BrowserAppLauncher>(profile_);

  if (!base::FeatureList::IsEnabled(kStopMojomAppService)) {
    app_service_mojom_impl_ =
        std::make_unique<apps::AppServiceMojomImpl>(profile_->GetPath());

    app_service_mojom_impl_->BindReceiver(
        app_service_.BindNewPipeAndPassReceiver());

    if (app_service_.is_connected()) {
      // The AppServiceProxy is a subscriber: something that wants to be able to
      // list all known apps.
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
      receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
      app_service_->RegisterSubscriber(std::move(subscriber), nullptr);
    }
  }
  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile_,
                              std::make_unique<apps::AppIconSource>(profile_));
}

AppPublisher* AppServiceProxyBase::GetPublisher(AppType app_type) {
  auto it = publishers_.find(app_type);
  return it == publishers_.end() ? nullptr : it->second;
}

mojo::Remote<apps::mojom::AppService>& AppServiceProxyBase::AppService() {
  return app_service_;
}

apps::AppRegistryCache& AppServiceProxyBase::AppRegistryCache() {
  return app_registry_cache_;
}

apps::AppCapabilityAccessCache&
AppServiceProxyBase::AppCapabilityAccessCache() {
  return app_capability_access_cache_;
}

BrowserAppLauncher* AppServiceProxyBase::BrowserAppLauncher() {
  return browser_app_launcher_.get();
}

apps::PreferredAppsListHandle& AppServiceProxyBase::PreferredAppsList() {
  return preferred_apps_list_;
}

void AppServiceProxyBase::RegisterPublisher(AppType app_type,
                                            AppPublisher* publisher) {
  publishers_[app_type] = publisher;
}

void AppServiceProxyBase::InitializePreferredAppsForAllSubscribers() {
  preferred_apps_list_.Init(
      preferred_apps_impl_->preferred_apps_list().GetValue());
}

void AppServiceProxyBase::OnPreferredAppsChanged(
    PreferredAppChangesPtr changes) {
  preferred_apps_list_.ApplyBulkUpdate(std::move(changes));
}

void AppServiceProxyBase::OnPreferredAppSet(
    const std::string& app_id,
    IntentFilterPtr intent_filter,
    IntentPtr intent,
    ReplacedAppPreferences replaced_app_preferences) {
  for (const auto& iter : publishers_) {
    iter.second->OnPreferredAppSet(
        app_id, intent_filter ? intent_filter->Clone() : nullptr,
        intent ? intent->Clone() : nullptr,
        CloneIntentFiltersMap(replaced_app_preferences));
  }
}

void AppServiceProxyBase::OnSupportedLinksPreferenceChanged(
    const std::string& app_id,
    bool open_in_app) {
  for (const auto& iter : publishers_) {
    iter.second->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
  }
}

void AppServiceProxyBase::OnSupportedLinksPreferenceChanged(
    AppType app_type,
    const std::string& app_id,
    bool open_in_app) {
  publishers_[app_type]->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
}

bool AppServiceProxyBase::HasPublisher(AppType app_type) {
  return base::Contains(publishers_, app_type);
}

absl::optional<IconKey> AppServiceProxyBase::GetIconKey(
    const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyBase::LoadIconFromIconKey(AppType app_type,
                                         const std::string& app_id,
                                         const IconKey& icon_key,
                                         IconType icon_type,
                                         int32_t size_hint_in_dip,
                                         bool allow_placeholder_icon,
                                         apps::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, icon_key, icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxyBase::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::LaunchSource launch_source,
                                 apps::WindowInfoPtr window_info) {
  app_registry_cache_.ForOneApp(
      app_id, [this, event_flags, launch_source,
               &window_info](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        if (MaybeShowLaunchPreventionDialog(update)) {
          return;
        }

        RecordAppLaunch(update.AppId(), launch_source);
        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 apps::LaunchContainer::kLaunchContainerNone);

        publisher->Launch(update.AppId(), event_flags, launch_source,
                          std::move(window_info));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource mojom_launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, mojom_launch_source,
                 &window_info](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          apps::LaunchSource launch_source =
              ConvertMojomLaunchSourceToLaunchSource(mojom_launch_source);
          RecordAppLaunch(update.AppId(), launch_source);
          RecordAppPlatformMetrics(profile_, update, launch_source,
                                   apps::LaunchContainer::kLaunchContainerNone);

          app_service_->Launch(ConvertAppTypeToMojomAppType(update.AppType()),
                               update.AppId(), event_flags, mojom_launch_source,
                               std::move(window_info));

          PerformPostLaunchTasks(launch_source);
        });
  }
}

void AppServiceProxyBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  app_registry_cache_.ForOneApp(
      app_id, [this, event_flags, launch_source,
               &file_paths](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        if (MaybeShowLaunchPreventionDialog(update)) {
          return;
        }

        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 LaunchContainer::kLaunchContainerNone);

        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }

        publisher->LaunchAppWithFiles(update.AppId(), event_flags,
                                      launch_source, std::move(file_paths));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource mojom_launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, mojom_launch_source,
                 &file_paths](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          apps::LaunchSource launch_source =
              ConvertMojomLaunchSourceToLaunchSource(mojom_launch_source);
          RecordAppPlatformMetrics(profile_, update, launch_source,
                                   apps::LaunchContainer::kLaunchContainerNone);

          // TODO(crbug/1117655): File manager records metrics for apps it
          // launched. So we only record launches from other places. We should
          // eventually move those metrics here, after AppService supports all
          // app types launched by file manager.
          if (launch_source != apps::LaunchSource::kFromFileManager) {
            RecordAppLaunch(update.AppId(), launch_source);
          }

          app_service_->LaunchAppWithFiles(
              ConvertAppTypeToMojomAppType(update.AppType()), update.AppId(),
              event_flags, mojom_launch_source, std::move(file_paths));

          PerformPostLaunchTasks(launch_source);
        });
  }
}

void AppServiceProxyBase::LaunchAppWithIntent(const std::string& app_id,
                                              int32_t event_flags,
                                              IntentPtr intent,
                                              LaunchSource launch_source,
                                              WindowInfoPtr window_info,
                                              LaunchCallback callback) {
  CHECK(intent);
  app_registry_cache_.ForOneApp(
      app_id,
      [this, event_flags, &intent, launch_source, &window_info,
       callback = std::move(callback)](const AppUpdate& update) mutable {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          std::move(callback).Run(LaunchResult(State::FAILED));
          return;
        }

        if (MaybeShowLaunchPreventionDialog(update)) {
          std::move(callback).Run(LaunchResult(State::FAILED));
          return;
        }

        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }
        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 LaunchContainer::kLaunchContainerNone);

        publisher->LaunchAppWithIntent(
            update.AppId(), event_flags, std::move(intent), launch_source,
            std::move(window_info), std::move(callback));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource mojom_launch_source,
    apps::mojom::WindowInfoPtr window_info,
    apps::mojom::Publisher::LaunchAppWithIntentCallback callback) {
  CHECK(intent);
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, &intent, mojom_launch_source, &window_info,
                 callback = std::move(callback)](
                    const apps::AppUpdate& update) mutable {
          if (MaybeShowLaunchPreventionDialog(update)) {
            if (callback)
              std::move(callback).Run(/*success=*/false);
            return;
          }

          apps::LaunchSource launch_source =
              ConvertMojomLaunchSourceToLaunchSource(mojom_launch_source);
          // TODO(crbug/1117655): File manager records metrics for apps it
          // launched. So we only record launches from other places. We should
          // eventually move those metrics here, after AppService supports all
          // app types launched by file manager.
          if (launch_source != apps::LaunchSource::kFromFileManager) {
            RecordAppLaunch(update.AppId(), launch_source);
          }
          RecordAppPlatformMetrics(profile_, update, launch_source,
                                   apps::LaunchContainer::kLaunchContainerNone);

          app_service_->LaunchAppWithIntent(
              ConvertAppTypeToMojomAppType(update.AppType()), update.AppId(),
              event_flags, std::move(intent), mojom_launch_source,
              std::move(window_info), std::move(callback));

          PerformPostLaunchTasks(launch_source);
        });
  } else if (callback) {
    std::move(callback).Run(/*success=*/false);
  }
}

void AppServiceProxyBase::LaunchAppWithUrl(const std::string& app_id,
                                           int32_t event_flags,
                                           GURL url,
                                           LaunchSource launch_source,
                                           WindowInfoPtr window_info) {
  LaunchAppWithIntent(
      app_id, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      launch_source, std::move(window_info), base::DoNothing());
}

void AppServiceProxyBase::LaunchAppWithUrlForBind(const std::string& app_id,
                                                  int32_t event_flags,
                                                  GURL url,
                                                  LaunchSource launch_source,
                                                  WindowInfoPtr window_info) {
  LaunchAppWithIntent(
      app_id, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      launch_source, std::move(window_info), base::DoNothing());
}

void AppServiceProxyBase::LaunchAppWithUrl(
    const std::string& app_id,
    int32_t event_flags,
    GURL url,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, apps_util::CreateIntentFromUrl(url),
                      launch_source, std::move(window_info), {});
}

void AppServiceProxyBase::LaunchAppWithParams(AppLaunchParams&& params,
                                              LaunchCallback callback) {
  auto app_type = app_registry_cache_.GetAppType(params.app_id);
  auto* publisher = GetPublisher(app_type);
  if (!publisher) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  app_registry_cache_.ForOneApp(
      params.app_id,
      [this, &params, &callback, &publisher](const apps::AppUpdate& update) {
        if (MaybeShowLaunchPreventionDialog(update)) {
          std::move(callback).Run(LaunchResult());
          return;
        }
        auto launch_source = params.launch_source;
        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != apps::LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }

        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 params.container);

        publisher->LaunchAppWithParams(
            std::move(params),
            base::BindOnce(&AppServiceProxyBase::OnLaunched,
                           weak_factory_.GetWeakPtr(), std::move(callback)));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::SetPermission(const std::string& app_id,
                                        PermissionPtr permission) {
  app_registry_cache_.ForOneApp(
      app_id, [this, &permission](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        publisher->SetPermission(update.AppId(), std::move(permission));
      });
}

void AppServiceProxyBase::SetPermission(const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(
              ConvertAppTypeToMojomAppType(update.AppType()), update.AppId(),
              std::move(permission));
        });
  }
}

void AppServiceProxyBase::UninstallSilently(const std::string& app_id,
                                            UninstallSource uninstall_source) {
  auto app_type = app_registry_cache_.GetAppType(app_id);
  auto* publisher = GetPublisher(app_type);
  if (!publisher) {
    return;
  }
  publisher->Uninstall(app_id, uninstall_source,
                       /*clear_site_data=*/false, /*report_abuse=*/false);
  PerformPostUninstallTasks(app_type, app_id, uninstall_source);
}

void AppServiceProxyBase::UninstallSilently(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  if (app_service_.is_connected()) {
    auto app_type = app_registry_cache_.GetAppType(app_id);
    app_service_->Uninstall(ConvertAppTypeToMojomAppType(app_type), app_id,
                            uninstall_source,
                            /*clear_site_data=*/false, /*report_abuse=*/false);
    PerformPostUninstallTasks(
        app_type, app_id,
        ConvertMojomUninstallSourceToUninstallSource(uninstall_source));
  }
}

void AppServiceProxyBase::StopApp(const std::string& app_id) {
  if (base::FeatureList::IsEnabled(kAppServiceWithoutMojom)) {
    auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
    if (publisher) {
      publisher->StopApp(app_id);
    }
    return;
  }

  if (!app_service_.is_connected()) {
    return;
  }
  auto app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->StopApp(ConvertAppTypeToMojomAppType(app_type), app_id);
}

void AppServiceProxyBase::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->GetMenuModel(app_id, menu_type, display_id, std::move(callback));
  } else {
    std::move(callback).Run(MenuItems());
  }
}

void AppServiceProxyBase::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    apps::mojom::Publisher::GetMenuModelCallback callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  auto app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->GetMenuModel(ConvertAppTypeToMojomAppType(app_type), app_id,
                             menu_type, display_id, std::move(callback));
}

void AppServiceProxyBase::ExecuteContextMenuCommand(
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  if (base::FeatureList::IsEnabled(kAppServiceWithoutMojom)) {
    auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
    if (publisher) {
      publisher->ExecuteContextMenuCommand(app_id, command_id, shortcut_id,
                                           display_id);
    }
    return;
  }

  if (!app_service_.is_connected()) {
    return;
  }

  auto app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->ExecuteContextMenuCommand(
      ConvertAppTypeToMojomAppType(app_type), app_id, command_id, shortcut_id,
      display_id);
}

void AppServiceProxyBase::OpenNativeSettings(const std::string& app_id) {
  if (base::FeatureList::IsEnabled(kAppServiceWithoutMojom)) {
    auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
    if (publisher) {
      publisher->OpenNativeSettings(app_id);
    }
    return;
  }

  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this](const apps::AppUpdate& update) {
          app_service_->OpenNativeSettings(
              ConvertAppTypeToMojomAppType(update.AppType()), update.AppId());
        });
  }
}

apps::IconLoader* AppServiceProxyBase::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

std::vector<std::string> AppServiceProxyBase::GetAppIdsForUrl(
    const GURL& url,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  auto intent_launch_info = GetAppsForIntent(
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      exclude_browsers, exclude_browser_tab_apps);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForIntent(
    const apps::IntentPtr& intent,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (!intent || intent->OnlyShareToDrive() || !intent->IsIntentValid()) {
    return intent_launch_info;
  }

  app_registry_cache_.ForEachApp([&intent_launch_info, &intent,
                                  &exclude_browsers, &exclude_browser_tab_apps](
                                     const apps::AppUpdate& update) {
    if (update.Readiness() != apps::Readiness::kReady &&
        update.Readiness() != apps::Readiness::kDisabledByPolicy) {
      // We consider apps disabled by policy to be ready as they cause URL
      // loads to be blocked.
      return;
    }
    if (!update.HandlesIntents().value_or(false)) {
      return;
    }
    if (exclude_browser_tab_apps &&
        update.WindowMode() == WindowMode::kBrowser) {
      return;
    }
    // |activity_label| -> {index, is_generic}
    std::map<std::string, IndexAndGeneric> best_handler_map;
    bool is_file_handling_intent = !intent->files.empty();
    size_t index = 0;
    for (const auto& filter : update.IntentFilters()) {
      DCHECK(filter);
      if (exclude_browsers && filter->IsBrowserFilter()) {
        continue;
      }
      if (intent->MatchFilter(filter)) {
        // Return the first non-generic match if it exists, otherwise the
        // first generic match.
        bool generic = false;
        if (is_file_handling_intent) {
          generic = apps_util::IsGenericFileHandler(intent, filter);
        }
        std::string activity_label = GetActivityLabel(filter, update);
        // Replace the best handler if it is generic and we have a non-generic
        // one.
        auto it = best_handler_map.find(activity_label);
        if (it == best_handler_map.end() ||
            (it->second.is_generic && !generic)) {
          best_handler_map[activity_label] = IndexAndGeneric{index, generic};
        }
      }
      index++;
    }
    const auto& filters = update.IntentFilters();
    for (const auto& handler_entry : best_handler_map) {
      const IntentFilterPtr& filter = filters[handler_entry.second.index];
      IntentLaunchInfo entry;
      entry.app_id = update.AppId();
      entry.activity_label = GetActivityLabel(filter, update);
      entry.activity_name = filter->activity_name.value_or("");
      entry.is_generic_file_handler =
          apps_util::IsGenericFileHandler(intent, filter);
      entry.is_file_extension_match = filter->IsFileExtensionsFilter();
      intent_launch_info.push_back(entry);
    }
  });
  return intent_launch_info;
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForFiles(
    std::vector<apps::IntentFilePtr> files) {
  return GetAppsForIntent(std::make_unique<apps::Intent>(
                              apps_util::kIntentActionView, std::move(files)),
                          false, false);
}

void AppServiceProxyBase::AddPreferredApp(const std::string& app_id,
                                          const GURL& url) {
  AddPreferredApp(app_id,
                  std::make_unique<Intent>(apps_util::kIntentActionView, url));
}

void AppServiceProxyBase::AddPreferredApp(const std::string& app_id,
                                          const IntentPtr& intent) {
  DCHECK(!app_id.empty());
  DCHECK(preferred_apps_impl_);

  auto intent_filter = FindBestMatchingFilter(intent);
  if (!intent_filter) {
    return;
  }

  // Treat kUseBrowserForLink like an app with a single supported link, so
  // that any apps with overlapping supported links will have their preference
  // removed correctly.
  if (app_id == apps_util::kUseBrowserForLink) {
    std::vector<IntentFilterPtr> filters;
    filters.push_back(std::move(intent_filter));
    preferred_apps_impl_->SetSupportedLinksPreference(AppType::kUnknown, app_id,
                                                      std::move(filters));
    return;
  }

  if (apps_util::IsSupportedLinkForApp(app_id, intent_filter)) {
    SetSupportedLinksPreference(app_id);
    return;
  }

  preferred_apps_list_.AddPreferredApp(app_id, intent_filter);
  preferred_apps_impl_->AddPreferredApp(
      app_registry_cache_.GetAppType(app_id), app_id, std::move(intent_filter),
      intent->Clone(), /*from_publisher=*/false);
}

void AppServiceProxyBase::SetSupportedLinksPreference(
    const std::string& app_id) {
  IntentFilters filters;
  AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const AppUpdate& app) {
        for (auto& filter : app.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });

  SetSupportedLinksPreference(app_id, std::move(filters));
}

void AppServiceProxyBase::SetSupportedLinksPreference(
    const std::string& app_id,
    IntentFilters all_link_filters) {
  DCHECK(!app_id.empty());

  preferred_apps_impl_->SetSupportedLinksPreference(
      app_registry_cache_.GetAppType(app_id), app_id,
      std::move(all_link_filters));
}

void AppServiceProxyBase::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  DCHECK(!app_id.empty());

  preferred_apps_impl_->RemoveSupportedLinksPreference(
      app_registry_cache_.GetAppType(app_id), app_id);
}

void AppServiceProxyBase::SetWindowMode(const std::string& app_id,
                                        WindowMode window_mode) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->SetWindowMode(app_id, window_mode);
  }
}

void AppServiceProxyBase::SetWindowMode(const std::string& app_id,
                                        apps::mojom::WindowMode window_mode) {
  if (app_service_.is_connected()) {
    app_service_->SetWindowMode(
        ConvertAppTypeToMojomAppType(app_registry_cache_.GetAppType(app_id)),
        app_id, window_mode);
  }
}

void AppServiceProxyBase::OnApps(std::vector<AppPtr> deltas,
                                 AppType app_type,
                                 bool should_notify_initialized) {
  for (const auto& delta : deltas) {
    if (delta->readiness != Readiness::kUnknown &&
        !apps_util::IsInstalled(delta->readiness)) {
      preferred_apps_impl_->RemovePreferredApp(delta->app_id);
    }
  }

  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                                 apps::mojom::AppType app_type,
                                 bool should_notify_initialized) {
  if (base::FeatureList::IsEnabled(kStopMojomAppService)) {
    return;
  }

  if (app_service_.is_connected()) {
    for (const auto& delta : deltas) {
      if (delta->readiness != apps::mojom::Readiness::kUnknown &&
          !apps_util::IsInstalled(delta->readiness)) {
        preferred_apps_impl_->RemovePreferredApp(delta->app_id);
      }
    }
  }

  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  app_capability_access_cache_.OnCapabilityAccesses(std::move(deltas));
}

void AppServiceProxyBase::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  app_capability_access_cache_.OnCapabilityAccesses(std::move(deltas));
}

void AppServiceProxyBase::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

IntentFilterPtr AppServiceProxyBase::FindBestMatchingFilter(
    const IntentPtr& intent) {
  IntentFilterPtr best_matching_intent_filter;
  if (!intent) {
    return best_matching_intent_filter;
  }

  int best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  app_registry_cache_.ForEachApp(
      [&intent, &best_match_level,
       &best_matching_intent_filter](const apps::AppUpdate& update) {
        for (auto& filter : update.IntentFilters()) {
          if (!intent->MatchFilter(filter)) {
            continue;
          }
          auto match_level = filter->GetFilterMatchLevel();
          if (match_level <= best_match_level) {
            continue;
          }
          best_matching_intent_filter = std::move(filter);
          best_match_level = match_level;
        }
      });
  return best_matching_intent_filter;
}

apps::mojom::IntentFilterPtr AppServiceProxyBase::FindBestMatchingMojomFilter(
    const apps::mojom::IntentPtr& mojom_intent) {
  apps::mojom::IntentFilterPtr best_matching_intent_filter;
  if (!app_service_.is_bound() || !mojom_intent) {
    return best_matching_intent_filter;
  }

  auto intent = ConvertMojomIntentToIntent(mojom_intent);
  if (!intent) {
    return best_matching_intent_filter;
  }
  int best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  app_registry_cache_.ForEachApp(
      [&intent, &best_match_level,
       &best_matching_intent_filter](const apps::AppUpdate& update) {
        for (const auto& filter : update.IntentFilters()) {
          if (!intent->MatchFilter(filter)) {
            continue;
          }
          auto match_level = filter->GetFilterMatchLevel();
          if (match_level <= best_match_level) {
            continue;
          }
          best_matching_intent_filter =
              ConvertIntentFilterToMojomIntentFilter(filter);
          best_match_level = match_level;
        }
      });
  return best_matching_intent_filter;
}

void AppServiceProxyBase::PerformPostLaunchTasks(
    apps::LaunchSource launch_source) {}

void AppServiceProxyBase::RecordAppPlatformMetrics(
    Profile* profile,
    const apps::AppUpdate& update,
    apps::LaunchSource launch_source,
    apps::LaunchContainer container) {}

void AppServiceProxyBase::PerformPostUninstallTasks(
    apps::AppType app_type,
    const std::string& app_id,
    UninstallSource uninstall_source) {}

void AppServiceProxyBase::OnLaunched(LaunchCallback callback,
                                     LaunchResult&& launch_result) {
  std::move(callback).Run(std::move(launch_result));
}

IntentLaunchInfo::IntentLaunchInfo() = default;
IntentLaunchInfo::~IntentLaunchInfo() = default;
IntentLaunchInfo::IntentLaunchInfo(const IntentLaunchInfo& other) = default;

}  // namespace apps
