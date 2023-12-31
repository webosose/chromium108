// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/backoff_entry.h"

class Profile;

// This class handles detection of child accounts (on sign-in as well as on
// browser restart), and triggers the appropriate behavior (e.g. enable the
// supervised user experience, fetch information about the parent(s)).
class ChildAccountServiceImpl : public ChildAccountService,
                                public FamilyInfoFetcher::Consumer,
                                public signin::IdentityManager::Observer,
                                public SupervisedUserService::Delegate {
 public:
  ChildAccountServiceImpl(const ChildAccountServiceImpl&) = delete;
  ChildAccountServiceImpl& operator=(const ChildAccountServiceImpl&) = delete;

  ~ChildAccountServiceImpl() override;

  void Init() override;

  bool IsChildAccountStatusKnown() override;

  // KeyedService:
  void Shutdown() override;

  void AddChildStatusReceivedCallback(base::OnceClosure callback) override;

  AuthState GetGoogleAuthState() override;

  base::CallbackListSubscription ObserveGoogleAuthState(
      const base::RepeatingCallback<void()>& callback) override;

 private:
  friend class ChildAccountServiceTest;
  friend class ChildAccountServiceFactory;
  // Use |ChildAccountServiceFactory::GetForProfile(...)| to get an instance of
  // this service.
  explicit ChildAccountServiceImpl(Profile* profile);

  // SupervisedUserService::Delegate implementation.
  void SetActive(bool active) override;

  // Sets whether the signed-in account is a child account.
  void SetIsChildAccount(bool is_child_account);

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // FamilyInfoFetcher::Consumer implementation.
  void OnGetFamilyMembersSuccess(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) override;
  void OnFailure(FamilyInfoFetcher::ErrorCode error) override;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  void StartFetchingFamilyInfo();
  void CancelFetchingFamilyInfo();
  void ScheduleNextFamilyInfoUpdate(base::TimeDelta delay);

  // Asserts that `is_child` matches the child status of the primary user.
  // Terminates user session in case of status mismatch in order to prevent
  // supervision incidents. Relevant on Chrome OS platform that has the concept
  // of the user.
  void AssertChildStatusOfTheUser(bool is_child);

  void SetFirstCustodianPrefs(const FamilyInfoFetcher::FamilyMember& custodian);
  void SetSecondCustodianPrefs(
      const FamilyInfoFetcher::FamilyMember& custodian);
  void ClearFirstCustodianPrefs();
  void ClearSecondCustodianPrefs();

  // Owns us via the KeyedService mechanism.
  raw_ptr<Profile> profile_;

  bool active_{false};

  std::unique_ptr<FamilyInfoFetcher> family_fetcher_;
  // If fetching the family info fails, retry with exponential backoff.
  base::OneShotTimer family_fetch_timer_;
  net::BackoffEntry family_fetch_backoff_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  base::RepeatingClosureList google_auth_state_observers_;

  // Callbacks to run when the user status becomes known.
  std::vector<base::OnceClosure> status_received_callback_list_;

  base::WeakPtrFactory<ChildAccountServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_IMPL_H_
