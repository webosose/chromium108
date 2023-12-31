// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_DEVICE_INFO_PROVIDER_H_
#define ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_DEVICE_INFO_PROVIDER_H_

#include "ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace ash {

namespace device_sync {

// Provides the cryptauth::GcmDeviceInfo object associated with the current
// device. cryptauth::GcmDeviceInfo describes properties of this Chromebook and
// is not expected to change except when the OS version is updated.
class GcmDeviceInfoProvider {
 public:
  GcmDeviceInfoProvider() = default;

  GcmDeviceInfoProvider(const GcmDeviceInfoProvider&) = delete;
  GcmDeviceInfoProvider& operator=(const GcmDeviceInfoProvider&) = delete;

  virtual ~GcmDeviceInfoProvider() = default;

  virtual const cryptauth::GcmDeviceInfo& GetGcmDeviceInfo() const = 0;
};

}  // namespace device_sync

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::device_sync {
using ::ash::device_sync::GcmDeviceInfoProvider;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_DEVICE_INFO_PROVIDER_H_
