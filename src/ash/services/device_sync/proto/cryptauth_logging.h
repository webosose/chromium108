// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_
#define ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_

#include <ostream>
#include <string>

#include "ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "base/values.h"

namespace cryptauthv2 {

std::string TruncateStringForLogs(const std::string& str);

std::string TargetServiceToString(TargetService service);
std::ostream& operator<<(std::ostream& stream, const TargetService& service);

std::string InvocationReasonToString(ClientMetadata::InvocationReason reason);
std::ostream& operator<<(std::ostream& stream,
                         const ClientMetadata::InvocationReason& reason);

std::string ConnectivityStatusToString(ConnectivityStatus status);
std::ostream& operator<<(std::ostream& stream,
                         const ConnectivityStatus& status);

base::Value PolicyReferenceToReadableDictionary(const PolicyReference& policy);
std::ostream& operator<<(std::ostream& stream, const PolicyReference& policy);

base::Value InvokeNextToReadableDictionary(const InvokeNext& invoke_next);
std::ostream& operator<<(std::ostream& stream, const InvokeNext& invoke_next);

base::Value ClientDirectiveToReadableDictionary(
    const ClientDirective& directive);
std::ostream& operator<<(std::ostream& stream,
                         const ClientDirective& directive);

base::Value DeviceMetadataPacketToReadableDictionary(
    const DeviceMetadataPacket& packet);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceMetadataPacket& packet);

base::Value EncryptedGroupPrivateKeyToReadableDictionary(
    const EncryptedGroupPrivateKey& key);
std::ostream& operator<<(std::ostream& stream,
                         const EncryptedGroupPrivateKey& key);

base::Value SyncMetadataResponseToReadableDictionary(
    const SyncMetadataResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const SyncMetadataResponse& response);

base::Value FeatureStatusToReadableDictionary(
    const DeviceFeatureStatus::FeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus::FeatureStatus& status);

base::Value DeviceFeatureStatusToReadableDictionary(
    const DeviceFeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus& status);

base::Value BatchGetFeatureStatusesResponseToReadableDictionary(
    const BatchGetFeatureStatusesResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const BatchGetFeatureStatusesResponse& response);

base::Value DeviceActivityStatusToReadableDictionary(
    const DeviceActivityStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceActivityStatus& status);

base::Value GetDevicesActivityStatusResponseToReadableDictionary(
    const GetDevicesActivityStatusResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const GetDevicesActivityStatusResponse& response);

base::Value BeaconSeedToReadableDictionary(const BeaconSeed& seed);
std::ostream& operator<<(std::ostream& stream, const BeaconSeed& seed);

base::Value BetterTogetherDeviceMetadataToReadableDictionary(
    const BetterTogetherDeviceMetadata& metadata);
std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherDeviceMetadata& metadata);

}  // namespace cryptauthv2

#endif  // ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_
