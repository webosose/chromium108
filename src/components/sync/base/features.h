// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_FEATURES_H_
#define COMPONENTS_SYNC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace syncer {

// Allows device registration within trusted vault server without having trusted
// vault key. Effectively disabled if kSyncTrustedVaultPassphraseRecovery
// is disabled.
BASE_DECLARE_FEATURE(kAllowSilentTrustedVaultDeviceRegistration);

// If enabled, EntitySpecifics will be cached in EntityMetadata in order to
// prevent data loss caused by older clients dealing with unknown proto fields
// (introduced later).
BASE_DECLARE_FEATURE(kCacheBaseEntitySpecificsInMetadata);

// Causes Sync to ignore updates encrypted with keys that have been missing for
// too long from this client; Sync will proceed normally as if those updates
// didn't exist.
BASE_DECLARE_FEATURE(kIgnoreSyncEncryptionKeysLongMissing);

// The threshold for kIgnoreSyncEncryptionKeysLongMissing to start ignoring keys
// (measured in number of GetUpdatesResponses messages).
inline constexpr base::FeatureParam<int> kMinGuResponsesToIgnoreKey{
    &kIgnoreSyncEncryptionKeysLongMissing, "MinGuResponsesToIgnoreKey", 3};

// Enables adding, displaying and modifying extra notes to stored credentials.
// When enabled, "PasswordViewPageInSettings" feature in the password manager
// codebase is ignored and the new password view subpage is force enabled. When
// enabled, Sync machinery will read and writes password notes to the
// `encrypted_notes_backup` field inside the PasswordSpecifics proto. Together
// with the logic on the server. this protects against notes being overwritten
// by legacy clients not supporting password notes.
// This feature is added here instead of the password manager codebase to avoid
// cycle dependencies.
BASE_DECLARE_FEATURE(kPasswordNotesWithBackup);
// Decides how long the user does not require reuathentication after
// successfully authenticated.
inline constexpr base::FeatureParam<base::TimeDelta> kPasswordNotesAuthValidity{
    &kPasswordNotesWithBackup, "authentication_validity_duration",
    base::Minutes(5)};

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
BASE_DECLARE_FEATURE(kSyncAllowWalletDataInTransportModeWithCustomPassphrase);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kSyncAndroidLimitNTPPromoImpressions);
inline constexpr base::FeatureParam<int> kSyncAndroidNTPPromoMaxImpressions{
    &kSyncAndroidLimitNTPPromoImpressions, "SyncAndroidNTPPromoMaxImpressions",
    5};
BASE_DECLARE_FEATURE(kSyncAndroidPromosWithAlternativeTitle);
BASE_DECLARE_FEATURE(kSyncAndroidPromosWithIllustration);
BASE_DECLARE_FEATURE(kSyncAndroidPromosWithSingleButton);
BASE_DECLARE_FEATURE(kSyncAndroidPromosWithTitle);
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Usage Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletUsageData);

// Causes the sync engine to count a quota for commits of data types that can
// be committed by extension JS API. If the quota is depleted, an extra long
// nudge delay is applied to that data type. As a result, more changes are
// likely to get combined into one commit message.
BASE_DECLARE_FEATURE(kSyncExtensionTypesThrottling);

BASE_DECLARE_FEATURE(kSyncResetPollIntervalOnStart);

// If enabled, interested data types, excluding Wallet and Offer, will be sent
// to the Sync Server as part of DeviceInfo.
BASE_DECLARE_FEATURE(kSyncSendInterestedDataTypes);

#if BUILDFLAG(IS_CHROMEOS)
// Whether warning should be shown in sync settings page when lacros
// side-by-side mode is enabled.
BASE_DECLARE_FEATURE(kSyncSettingsShowLacrosSideBySideWarning);

// Whether explicit passphrase sharing between Ash and Lacros is enabled.
BASE_DECLARE_FEATURE(kSyncChromeOSExplicitPassphraseSharing);

// Whether Apps toggle value is exposed by Ash to Lacros.
BASE_DECLARE_FEATURE(kSyncChromeOSAppsToggleSharing);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether the periodic degraded recoverability polling is enabled.
BASE_DECLARE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling);
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling",
        base::Days(7)};
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling",
        base::Hours(1)};

#if BUILDFLAG(IS_IOS)
// Whether RPC is enabled.
BASE_DECLARE_FEATURE(kSyncTrustedVaultPassphraseiOSRPC);
#endif  // BUILDFLAG(IS_IOS)

// Whether the entry point to opt in to trusted vault in settings should be
// shown.
BASE_DECLARE_FEATURE(kSyncTrustedVaultPassphrasePromo);

BASE_DECLARE_FEATURE(kSyncTrustedVaultPassphraseRecovery);
// Specifies how long requests to vault service shouldn't be retried after
// encountering transient error.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTrustedVaultServiceThrottlingDuration{
        &kSyncTrustedVaultPassphraseRecovery,
        "TrustedVaultServiceThrottlingDuration", base::Days(1)};

// Enables logging a UMA metric that requires first communicating with the
// trusted vault server, in order to verify that the local notion of the device
// being registered is consistent with the server-side state.
BASE_DECLARE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration);

// Triggers another device registration attempt if the device was registered
// before this feature was introduced.
BASE_DECLARE_FEATURE(kSyncTrustedVaultRedoDeviceRegistration);

// Triggers one-off reset of `keys_are_stale`, allowing another device
// registration attempt if previous was failed.
BASE_DECLARE_FEATURE(kSyncTrustedVaultResetKeysAreStale);

// Enables storing MD5 hashed trusted vault file instead of OSCrypt encrypted.
BASE_DECLARE_FEATURE(kSyncTrustedVaultUseMD5HashedFile);

// If enabled, the device will register with FCM and listen to new
// invalidations. Also, FCM token will be set in DeviceInfo, which signals to
// the server that device listens to new invalidations.
// The device will not subscribe to old invalidations for any data types except
// Wallet and Offer, since that will be covered by the new system.
// SyncSendInterestedDataTypes must be enabled for this to take effect.
BASE_DECLARE_FEATURE(kUseSyncInvalidations);

// If enabled, types related to Wallet and Offer will be included in interested
// data types, and the device will listen to new invalidations for those types
// (if they are enabled).
// The device will not register for old invalidations at all.
// UseSyncInvalidations must be enabled for this to take effect.
BASE_DECLARE_FEATURE(kUseSyncInvalidationsForWalletAndOffer);

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
BASE_DECLARE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

#if BUILDFLAG(IS_IOS)
// Returns whether RPC is enabled.
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled();
#endif  // BUILDFLAG(IS_IOS)

BASE_DECLARE_FEATURE(kSyncEnableHistoryDataType);

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataType);

BASE_DECLARE_FEATURE(kSyncPauseUponAnyPersistentAuthError);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
