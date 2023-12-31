// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center.h"

namespace {

const char kDiscoveryLearnMoreLink[] =
    "https://support.google.com/chromebook?p=fast_pair_m101";
const char kAssociateAccountLearnMoreLink[] =
    "https://support.google.com/chromebook?p=bluetooth_pairing_m101";

bool ShouldShowUserEmail(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairPresenterImpl::Factory*
    FastPairPresenterImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairPresenter> FastPairPresenterImpl::Factory::Create(
    message_center::MessageCenter* message_center) {
  if (g_test_factory_)
    return g_test_factory_->CreateInstance(message_center);

  return base::WrapUnique(new FastPairPresenterImpl(message_center));
}

// static
void FastPairPresenterImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPresenterImpl::Factory::~Factory() = default;

FastPairPresenterImpl::FastPairPresenterImpl(
    message_center::MessageCenter* message_center)
    : notification_controller_(
          std::make_unique<FastPairNotificationController>(message_center)) {}

FastPairPresenterImpl::~FastPairPresenterImpl() = default;

void FastPairPresenterImpl::AddDeviceToDiscoveryNotificationAlreadyShownMap(
    scoped_refptr<Device> device) {
  DevicesWithDiscoveryNotificationAlreadyShown device_with_discovery_shown;
  device_with_discovery_shown.protocol = device->protocol;
  device_with_discovery_shown.metadata_id = device->metadata_id;
  address_to_devices_with_discovery_notification_already_shown_map_
      .insert_or_assign(device->ble_address, device_with_discovery_shown);
}

void FastPairPresenterImpl::ShowDiscovery(scoped_refptr<Device> device,
                                          DiscoveryCallback callback) {
  DCHECK(device);

  // If we are currently running a timer for a recently lost device that we
  // are already preventing notifications for, return early.
  if (base::Contains(address_to_lost_device_timer_map_, device->ble_address)) {
    callback.Run(DiscoveryAction::kAlreadyDisplayed);
    return;
  }

  // If we have already shown a discovery notification for a device in the
  // same protocol, don't show one again. This prevents a notification being
  // dismissed and reappearing for every advertisement for some devices.
  if (WasDiscoveryNotificationAlreadyShownForDevice(*device)) {
    callback.Run(DiscoveryAction::kAlreadyDisplayed);
    return;
  }

  AddDeviceToDiscoveryNotificationAlreadyShownMap(device);

  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id, base::BindRepeating(
                       &FastPairPresenterImpl::OnDiscoveryMetadataRetrieved,
                       weak_pointer_factory_.GetWeakPtr(), device, callback));
}

bool FastPairPresenterImpl::WasDiscoveryNotificationAlreadyShownForDevice(
    const Device& device) {
  auto it =
      address_to_devices_with_discovery_notification_already_shown_map_.find(
          device.ble_address);
  if (it ==
      address_to_devices_with_discovery_notification_already_shown_map_.end())
    return false;

  DevicesWithDiscoveryNotificationAlreadyShown device_with_discovery_shown =
      it->second;
  return device.metadata_id == device_with_discovery_shown.metadata_id &&
         device.protocol == device_with_discovery_shown.protocol;
}

void FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata)
    return;

  if (device->protocol == Protocol::kFastPairSubsequent) {
    ShowSubsequentDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
  // available then the device is v1.
  if (device_metadata->GetDetails()
          .anti_spoofing_key_pair()
          .public_key()
          .empty()) {
    device->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion,
                              {1});
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion1);
  } else {
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion2);
  }

  // If we are in guest-mode, or are missing the IdentifyManager needed to show
  // detailed user notification, show the guest notification. We don't have to
  // verify opt-in status in this case because Guests will be guaranteed to not
  // have opt-in status.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager ||
      !ShouldShowUserEmail(
          Shell::Get()->session_controller()->login_status())) {
    QP_LOG(VERBOSE) << __func__
                    << ": in guest mode, showing guest notification";
    ShowGuestDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  // Check if the user is opted in to saving devices to their account. If the
  // user is not opted in, we will show the guest notification which does not
  // mention saving devices to the user account. This is flagged depending if
  // the Fast Pair Saved Devices is enabled and we are using a strict
  // interpretation of the opt-in status.
  if (features::IsFastPairSavedDevicesEnabled() &&
      features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    FastPairRepository::Get()->CheckOptInStatus(base::BindOnce(
        &FastPairPresenterImpl::OnCheckOptInStatus,
        weak_pointer_factory_.GetWeakPtr(), device, callback, device_metadata));
    return;
  }

  // If we don't have SavedDevices flag enabled, then we can ignore the user's
  // opt in status and move forward to showing the User Discovery notification.
  ShowUserDiscoveryNotification(device, callback, device_metadata);
}

void FastPairPresenterImpl::OnCheckOptInStatus(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata,
    nearby::fastpair::OptInStatus status) {
  QP_LOG(INFO) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    ShowGuestDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  ShowUserDiscoveryNotification(device, callback, device_metadata);
}

void FastPairPresenterImpl::ShowSubsequentDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  if (!device_metadata)
    return;

  // Since Subsequent Pairing scenario can only happen for a signed in user
  // when a device has already been saved to their account, this should never
  // be null. We cannot get to this scenario in Guest Mode.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  DCHECK(identity_manager);

  const std::string& email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  notification_controller_->ShowSubsequentDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowGuestDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  notification_controller_->ShowGuestDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowUserDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  // Since we check this in |OnInitialDiscoveryMetadataRetrieved| to determine
  // if we should show the Guest notification, this should never be null.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  DCHECK(identity_manager);

  const std::string& email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  notification_controller_->ShowUserDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnDiscoveryClicked(DiscoveryCallback callback) {
  callback.Run(DiscoveryAction::kPairToDevice);
}

void FastPairPresenterImpl::OnDiscoveryDismissed(scoped_refptr<Device> device,
                                                 DiscoveryCallback callback,
                                                 bool user_dismissed) {
  // If the discovery notification was not dismissed by user, we remove the
  // device from the map in order to allow the notification to show again. We
  // check |WasDiscoveryNotificationAlreadyShownForDevice| to make sure it is
  // the same protocol, address, and metadata in the map before removing to
  // prevent edge cases (for example, a device changes protocol but uses the
  // same address).
  if (!user_dismissed &&
      WasDiscoveryNotificationAlreadyShownForDevice(*device)) {
    address_to_devices_with_discovery_notification_already_shown_map_.erase(
        device->ble_address);
  }

  callback.Run(user_dismissed ? DiscoveryAction::kDismissedByUser
                              : DiscoveryAction::kDismissed);
}

void FastPairPresenterImpl::StartDeviceLostTimer(scoped_refptr<Device> device) {
  auto [it, was_emplaced] = address_to_lost_device_timer_map_.try_emplace(
      device->ble_address, std::make_unique<base::OneShotTimer>());

  // If device is already in the map, return early. This means that the timer
  // has not expired yet for the device, since we erase the map instance
  // when the timer expires.
  if (!was_emplaced)
    return;

  // Start timer for how long to keep the device in
  // |address_to_devices_with_discovery_notification_already_shown_map_|, which
  // prevents the discovery notifications from showing up for the device.
  // When |AllowNotificationForRecentlyLostDevice| is fired on timeout, remove
  // the device from the map, allowing notifications to appear again.
  QP_LOG(VERBOSE) << __func__ << device;
  it->second->Start(
      FROM_HERE,
      base::Minutes(
          features::kFastPairDeviceLostNotificationTimeoutMinutes.Get()),
      base::BindOnce(
          &FastPairPresenterImpl::AllowNotificationForRecentlyLostDevice,
          weak_pointer_factory_.GetWeakPtr(), device));
}

void FastPairPresenterImpl::AllowNotificationForRecentlyLostDevice(
    scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << device;
  // We check that |device| is in
  // |address_to_devices_with_discovery_notification_already_shown_map_| before
  // we erase the timer and discovery notification to prevent the edge case
  // where a device has the same address as a device already in our map. This
  // happens with JBL 650s when they switch from initial to subsequent pairing.
  if (WasDiscoveryNotificationAlreadyShownForDevice(*device)) {
    QP_LOG(VERBOSE) << __func__
                    << ": allowing notifications again for device=" << device;
    address_to_lost_device_timer_map_.erase(device->ble_address);
    address_to_devices_with_discovery_notification_already_shown_map_.erase(
        device->ble_address);
  }
}

void FastPairPresenterImpl::OnDiscoveryLearnMoreClicked(
    DiscoveryCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kDiscoveryLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  callback.Run(DiscoveryAction::kLearnMore);
}

void FastPairPresenterImpl::ShowPairing(scoped_refptr<Device> device) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device));
}

void FastPairPresenterImpl::OnPairingMetadataRetrieved(
    scoped_refptr<Device> device,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowPairingNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(), base::DoNothing());
}

void FastPairPresenterImpl::ShowPairingFailed(scoped_refptr<Device> device,
                                              PairingFailedCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnPairingFailedMetadataRetrieved(
    scoped_refptr<Device> device,
    PairingFailedCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowErrorNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnNavigateToSettings,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnNavigateToSettings(
    PairingFailedCallback callback) {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
    RecordNavigateToSettingsResult(/*success=*/true);
  } else {
    QP_LOG(WARNING) << "Cannot open Bluetooth Settings since it's not possible "
                       "to opening WebUI settings";
    RecordNavigateToSettingsResult(/*success=*/false);
  }

  callback.Run(PairingFailedAction::kNavigateToSettings);
}

void FastPairPresenterImpl::OnPairingFailedDismissed(
    PairingFailedCallback callback,
    bool user_dismissed) {
  callback.Run(user_dismissed ? PairingFailedAction::kDismissedByUser
                              : PairingFailedAction::kDismissed);
}

void FastPairPresenterImpl::ShowAssociateAccount(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  QP_LOG(VERBOSE) << __func__ << ": " << device;
  if (!device_metadata) {
    return;
  }

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager) {
    QP_LOG(ERROR) << __func__
                  << ": IdentityManager is not available for Associate Account "
                     "notification.";
    return;
  }

  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;

  notification_controller_->ShowAssociateAccount(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountActionClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnAssociateAccountDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnAssociateAccountActionClicked(
    AssociateAccountCallback callback) {
  callback.Run(AssociateAccountAction::kAssoicateAccount);
}

void FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked(
    AssociateAccountCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kAssociateAccountLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  callback.Run(AssociateAccountAction::kLearnMore);
}

void FastPairPresenterImpl::OnAssociateAccountDismissed(
    AssociateAccountCallback callback,
    bool user_dismissed) {
  callback.Run(user_dismissed ? AssociateAccountAction::kDismissedByUser
                              : AssociateAccountAction::kDismissed);
}

void FastPairPresenterImpl::ShowCompanionApp(scoped_refptr<Device> device,
                                             CompanionAppCallback callback) {}

void FastPairPresenterImpl::RemoveNotifications(
    bool clear_already_shown_discovery_notification_cache) {
  if (clear_already_shown_discovery_notification_cache) {
    address_to_devices_with_discovery_notification_already_shown_map_.clear();
  }

  notification_controller_->RemoveNotifications();
}

void FastPairPresenterImpl::
    RemoveDeviceFromAlreadyShownDiscoveryNotificationCache(
        scoped_refptr<Device> device) {
  address_to_devices_with_discovery_notification_already_shown_map_.erase(
      device->ble_address);
}

}  // namespace quick_pair
}  // namespace ash
