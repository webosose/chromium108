// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_
#define ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_

#include "ash/services/device_sync/network_request_error.h"
#include "ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<ash::device_sync::mojom::NetworkRequestResult,
                     ash::device_sync::NetworkRequestError> {
  static ash::device_sync::mojom::NetworkRequestResult Convert(
      ash::device_sync::NetworkRequestError type);
};

}  // namespace mojo

#endif  // ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_
