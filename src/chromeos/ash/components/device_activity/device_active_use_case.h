// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVE_USE_CASE_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVE_USE_CASE_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

class PrefService;

namespace chromeos::system {
class StatisticsProvider;
}  // namespace chromeos::system

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

// Fields used in setting device active metadata, that are explicitly
// required from outside of ASH_CHROME due to the dependency limitations
// on chrome browser.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    ChromeDeviceMetadataParameters {
  version_info::Channel chromeos_channel;
  MarketSegment market_segment;
};

// Create a delegate which can be used to create fakes in unit tests.
// Fake via. delegate is required for creating deterministic unit tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    PsmDelegateInterface {
 public:
  virtual ~PsmDelegateInterface() = default;
  virtual rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) = 0;
};

// Base class for device active use cases.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    DeviceActiveUseCase {
 public:
  DeviceActiveUseCase(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      const std::string& use_case_pref_key,
      private_membership::rlwe::RlweUseCase psm_use_case,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  DeviceActiveUseCase(const DeviceActiveUseCase&) = delete;
  DeviceActiveUseCase& operator=(const DeviceActiveUseCase&) = delete;
  virtual ~DeviceActiveUseCase();

  // Generate the window identifier for the use case.
  // Granularity of formatted date will be based on the use case.
  //
  // Method is called to generate |window_id_| every time the machine
  // transitions out of the idle state. When reporting the use case is
  // completed for a use case, the |window_id_| is reset to absl::nullopt.
  virtual std::string GenerateUTCWindowIdentifier(base::Time ts) const = 0;

  // Generate Fresnel PSM import request body.
  // This will create the device metadata dimensions sent by PSM import by use
  // case.
  //
  // Important: Each new dimension added to metadata will need to be approved by
  // privacy.
  virtual ImportDataRequest GenerateImportRequestBody() = 0;

  // Method used to reset the non constant saved state of the device active use
  // case. The state should be cleared after reporting device actives.
  void ClearSavedState();

  PrefService* GetLocalState() const;

  // Return the last known ping timestamp from local state pref, by use case.
  // For example, the monthly use case will return the last known monthly
  // timestamp from the local state pref.
  base::Time GetLastKnownPingTimestamp() const;

  // Set the last known ping timestamp in local state pref.
  void SetLastKnownPingTimestamp(base::Time new_ts);

  // Return true if the |use_case_pref_key_| is not Unix Epoch (default value).
  bool IsLastKnownPingTimestampSet() const;

  // Retrieve the PSM use case.
  // The PSM dataset on the serverside is segmented by the PSM use case.
  private_membership::rlwe::RlweUseCase GetPsmUseCase() const;

  absl::optional<std::string> GetWindowIdentifier() const;

  // Updates the window identifier, which updates the |psm_id_| and
  // |psm_rlwe_client_|.
  bool SetWindowIdentifier(base::Time ts);

  // This method will return nullopt if this method is called before the window
  // identifier was set successfully.
  absl::optional<private_membership::rlwe::RlwePlaintextId> GetPsmIdentifier()
      const;

  // Calculates an HMAC of |message| using |key|, encoded as a hexadecimal
  // string. Return empty string if HMAC fails.
  std::string GetDigestString(const std::string& key,
                              const std::string& message) const;

  // Returns memory address to the |psm_rlwe_client_| unique pointer, or null if
  // not set.
  private_membership::rlwe::PrivateMembershipRlweClient* GetPsmRlweClient();

  // Determine if a device ping is needed for a given device window.
  // Performing this check helps reduce QPS to the |CheckingMembership|
  // network requests.
  //
  // The first active use case will always return true since the window
  // identifier is constant.
  virtual bool IsDevicePingRequired(base::Time new_ping_ts) const;

  // Generates the AES-256 encrypted ciphertext, which is used to store
  // the timestamp for only the first active use case.
  // The device stable secret (only known to the chromebook itself) is needed to
  // encrypt/decrypt this value. This ensures the first active timestamp is
  // reversible by only the device itself.
  virtual bool EncryptPsmValueAsCiphertext(base::Time ts);

  // Retrieves and decrypts the AES-256 encrypted psm value to a timestamp.
  virtual base::Time DecryptPsmValueAsTimestamp(std::string ciphertext) const;

 protected:
  // Retrieve full hardware class from MachineStatistics.
  // |DeviceActivityController| waits for object to finish loading, to avoid
  // callback logic in this class.
  std::string GetFullHardwareClass() const;

  // Retrieve the ChromeOS major version number.
  std::string GetChromeOSVersion() const;

  // Retrieve the ChromeOS release channel.
  Channel GetChromeOSChannel() const;

  // Retrieve the ChromeOS device market segment.
  MarketSegment GetMarketSegment() const;

  // Retrieve |psm_device_active_secret_|.
  const std::string& GetPsmDeviceActiveSecret() const;

 private:
  // Uniquely identifies a window of time for device active counting.
  //
  // Generated on demand each time the |window_id_| is regenerated.
  // This field is used apart of PSM Oprf, Query, and Import requests.
  absl::optional<private_membership::rlwe::RlwePlaintextId>
  GeneratePsmIdentifier(absl::optional<std::string> window_id) const;

  // Regenerated each time the state machine leaves the idle state.
  // Client Generates protos used in request body of Oprf and Query requests.
  void SetPsmRlweClient(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client);

  // The ChromeOS platform code will provide a derived PSM device active secret
  // via callback.
  //
  // This secret is used to generate a PSM identifier for the reporting window.
  const std::string psm_device_active_secret_;

  // Creates a copy of chrome parameters, which is owned throughout
  // |DeviceActiveUseCase| object lifetime.
  const ChromeDeviceMetadataParameters chrome_passed_device_params_;

  // Key used to query the local state pref for the last ping timestamp by use
  // case.
  // For example, the monthly use case will store the key mapping to the last
  // monthly ping timestamp in the local state pref.
  const std::string use_case_pref_key_;

  // The PSM dataset on the serverside is segmented by the PSM use case.
  const private_membership::rlwe::RlweUseCase psm_use_case_;

  // Update last stored device active ping timestamps for PSM use cases.
  // On powerwash/recovery update |local_state_| to the most recent timestamp
  // |CheckMembership| was performed, as |local_state_| gets deleted.
  // |local_state_| outlives the lifetime of this class.
  // Used local state prefs are initialized by |DeviceActivityController|.
  PrefService* const local_state_;

  // Abstract class used to generate the |psm_rlwe_client_|.
  std::unique_ptr<PsmDelegateInterface> psm_delegate_;

  // Singleton lives throughout class lifetime.
  chromeos::system::StatisticsProvider* const statistics_provider_;

  // Generated on demand each time the state machine leaves the idle state.
  // This field is used to know which window the psm id is used for.
  absl::optional<std::string> window_id_;

  // Generated on demand each time the state machine leaves the idle state.
  // It is reused by several states. It is reset to nullopt.
  // This field is used apart of PSM Oprf, Query, and Import requests.
  absl::optional<private_membership::rlwe::RlwePlaintextId> psm_id_;

  // Generated on demand each time the state machine leaves the idle state.
  // Client Generates protos used in request body of Oprf and Query requests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVE_USE_CASE_H_
