// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"

#include "ash/constants/ash_pref_names.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {

// static
SystemWebAppManager* SystemWebAppManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SystemWebAppManager*>(
      SystemWebAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
SystemWebAppManagerFactory* SystemWebAppManagerFactory::GetInstance() {
  return base::Singleton<SystemWebAppManagerFactory>::get();
}

// static
bool SystemWebAppManagerFactory::IsServiceCreatedForProfile(Profile* profile) {
  return SystemWebAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
             profile, /*create=*/false) != nullptr;
}

SystemWebAppManagerFactory::SystemWebAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SystemWebAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

SystemWebAppManagerFactory::~SystemWebAppManagerFactory() = default;

KeyedService* SystemWebAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(web_app::WebAppProviderFactory::IsServiceCreatedForProfile(profile));

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  DCHECK(provider);

  SystemWebAppManager* swa_manager = new SystemWebAppManager(profile);
  swa_manager->ConnectSubsystems(provider);
  swa_manager->ScheduleStart();

  return swa_manager;
}

bool SystemWebAppManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* SystemWebAppManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return web_app::GetBrowserContextForWebApps(context);
}

void SystemWebAppManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSystemWebAppLastUpdateVersion, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastInstalledLocale, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastAttemptedVersion, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastAttemptedLocale, "");
  registry->RegisterIntegerPref(prefs::kSystemWebAppInstallFailureCount, 0);
}

}  //  namespace ash
