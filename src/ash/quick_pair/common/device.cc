// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/device.h"

#include <ostream>

#include "ash/quick_pair/common/protocol.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"

namespace {

std::ostream& OutputToStream(std::ostream& stream,
                             const std::string& metadata_id,
                             const std::string& ble_address,
                             const absl::optional<std::string>& classic_address,
                             const ash::quick_pair::Protocol& protocol) {
  stream << "[Device: metadata_id=" << metadata_id;

  // We can only include PII from the device in verbose logging.
  if (VLOG_IS_ON(/*verbose_level=*/1)) {
    stream << ", ble_address=" << ble_address
           << ", class_address=" << classic_address.value_or("null");
  }

  stream << ", protocol=" << protocol << "]";
  return stream;
}

}  // namespace

namespace ash {
namespace quick_pair {

Device::Device(std::string metadata_id,
               std::string ble_address,
               Protocol protocol)
    : metadata_id(std::move(metadata_id)),
      ble_address(std::move(ble_address)),
      protocol(protocol) {}

Device::~Device() = default;

absl::optional<std::vector<uint8_t>> Device::GetAdditionalData(
    const AdditionalDataType& type) const {
  auto it = additional_data_.find(type);

  if (it == additional_data_.end())
    return absl::nullopt;

  return it->second;
}

void Device::SetAdditionalData(const AdditionalDataType& type,
                               const std::vector<uint8_t>& data) {
  auto result = additional_data_.emplace(type, data);

  if (type == AdditionalDataType::kFastPairVersion && data[0] == 1)
    set_classic_address(ble_address);

  if (!result.second) {
    result.first->second = data;
  }
}

std::ostream& operator<<(std::ostream& stream, const Device& device) {
  return OutputToStream(stream, device.metadata_id, device.ble_address,
                        device.classic_address(), device.protocol);
}

std::ostream& operator<<(std::ostream& stream, scoped_refptr<Device> device) {
  return OutputToStream(stream, device->metadata_id, device->ble_address,
                        device->classic_address(), device->protocol);
}

}  // namespace quick_pair
}  // namespace ash
