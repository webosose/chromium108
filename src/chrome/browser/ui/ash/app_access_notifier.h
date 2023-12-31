// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_ACCESS_NOTIFIER_H_
#define CHROME_BROWSER_UI_ASH_APP_ACCESS_NOTIFIER_H_

#include <list>
#include <map>
#include <string>

#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
class AppCapabilityAccessCache;
class AppRegistryCache;
}  // namespace apps

// This class is responsible for observing AppCapabilityAccessCache, notifying
// to appropriate entities when an app is accessing camera/microphone. This is
// also the concrete implementation of MicrophoneMuteNotificationDelegate, which
// allows code relevant to microphone mute notifications that resides under
// //ash to invoke functions/objects that actually reside under //chrome.
class AppAccessNotifier
    : public ash::MicrophoneMuteNotificationDelegate,
      public apps::AppCapabilityAccessCache::Observer,
      public session_manager::SessionManagerObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  AppAccessNotifier();
  AppAccessNotifier(const AppAccessNotifier&) = delete;
  AppAccessNotifier& operator=(const AppAccessNotifier&) = delete;
  ~AppAccessNotifier() override;

  // ash::MicrophoneMuteNotificationDelegate
  absl::optional<std::u16string> GetAppAccessingMicrophone() override;

  // apps::AppCapabilityAccessCache::Observer
  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& update) override;
  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  // user_manager::UserManager::UserSessionStateObserver
  void ActiveUserChanged(user_manager::User* active_user) override;

  // Get the app short name of the app with `app_id`.
  static absl::optional<std::u16string> GetAppShortNameFromAppId(
      std::string app_id,
      apps::AppRegistryCache* registry_cache);

 protected:
  // Returns the active user's account ID if we have an active user, an empty
  // account ID otherwise.
  virtual AccountId GetActiveUserAccountId();

  // Compares the active user's account ID to our last known value and, if the
  // ID has changed, then updates the AppCapabilityAccessCache that we observe
  // as well as the last known account ID.
  void CheckActiveUserChanged();

 private:
  friend class AppAccessNotifierBaseTest;

  // Returns the AppCapabilityAccessCache associated with the active user's
  // account ID.
  apps::AppCapabilityAccessCache* GetActiveUserAppCapabilityAccessCache();

  // Returns the "short name" of the registered app to most recently attempt to
  // access the microphone, or an empty (optional) string if none exists. Used
  // for the microphone mute notification.
  absl::optional<std::u16string> GetMostRecentAppAccessingMicrophone(
      apps::AppCapabilityAccessCache* capability_cache,
      apps::AppRegistryCache* registry_cache);

  // List of IDs of apps that have attempted to use the microphone, in order of
  // most-recently-launched.
  using MruAppIdList = std::list<std::string>;

  // Each user has their own list of MRU apps.  It's intended to persist across
  // multiple logouts/logins, and we specifically don't ever clear it. This is
  // used for the microphone mute notification.
  using MruAppIdMap = std::map<AccountId, MruAppIdList>;
  MruAppIdMap mic_using_app_ids;

  // Account ID of the last known active user.
  AccountId active_user_account_id_ = EmptyAccountId();

  // Observations.
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver,
                          &session_manager::SessionManager::AddObserver,
                          &session_manager::SessionManager::RemoveObserver>
      session_manager_observation_{this};
  base::ScopedObservation<
      user_manager::UserManager,
      user_manager::UserManager::UserSessionStateObserver,
      &user_manager::UserManager::AddSessionStateObserver,
      &user_manager::UserManager::RemoveSessionStateObserver>
      user_session_state_observation_{this};
  base::ScopedObservation<apps::AppCapabilityAccessCache,
                          apps::AppCapabilityAccessCache::Observer>
      app_capability_access_cache_observation_{this};

  base::WeakPtrFactory<AppAccessNotifier> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_APP_ACCESS_NOTIFIER_H_
