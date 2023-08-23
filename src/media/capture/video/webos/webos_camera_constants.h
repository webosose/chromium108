// Copyright 2022 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_CONSTANTS_H_
#define MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_CONSTANTS_H_

#include "media/capture/capture_export.h"

namespace media {

// webOS camera service method names
CAPTURE_EXPORT extern const char kOpen[];
CAPTURE_EXPORT extern const char kClose[];
CAPTURE_EXPORT extern const char kGetCameraList[];
CAPTURE_EXPORT extern const char kGetInfo[];
CAPTURE_EXPORT extern const char kGetProperties[];
CAPTURE_EXPORT extern const char kSetProperties[];
CAPTURE_EXPORT extern const char kSetFormat[];
CAPTURE_EXPORT extern const char kStartPreview[];
CAPTURE_EXPORT extern const char kStopPreview[];
CAPTURE_EXPORT extern const char kGetEventNotification[];

// webOS camera service payload key names
CAPTURE_EXPORT extern const char kKey[];
CAPTURE_EXPORT extern const char kSig[];
CAPTURE_EXPORT extern const char kYUV[];
CAPTURE_EXPORT extern const char kJPEG[];
CAPTURE_EXPORT extern const char kNV12[];
CAPTURE_EXPORT extern const char kNV21[];
CAPTURE_EXPORT extern const char kPan[];
CAPTURE_EXPORT extern const char kTilt[];
CAPTURE_EXPORT extern const char kZoom[];
CAPTURE_EXPORT extern const char kId[];
CAPTURE_EXPORT extern const char kReturnValue[];
CAPTURE_EXPORT extern const char kDeviceList[];
CAPTURE_EXPORT extern const char kInfo[];
CAPTURE_EXPORT extern const char kName[];
CAPTURE_EXPORT extern const char kMode[];
CAPTURE_EXPORT extern const char kPid[];
CAPTURE_EXPORT extern const char kHandle[];
CAPTURE_EXPORT extern const char kParams[];
CAPTURE_EXPORT extern const char kWidth[];
CAPTURE_EXPORT extern const char kHeight[];
CAPTURE_EXPORT extern const char kFormat[];
CAPTURE_EXPORT extern const char kFps[];
CAPTURE_EXPORT extern const char kType[];
CAPTURE_EXPORT extern const char kPosixShm[];
CAPTURE_EXPORT extern const char kSharedMemory[];
CAPTURE_EXPORT extern const char kSource[];
CAPTURE_EXPORT extern const char kResolution[];
CAPTURE_EXPORT extern const char kFrequency[];
CAPTURE_EXPORT extern const char kMax[];
CAPTURE_EXPORT extern const char kMin[];
CAPTURE_EXPORT extern const char kValue[];
CAPTURE_EXPORT extern const char kStep[];
CAPTURE_EXPORT extern const char kWhiteBalanceTemperature[];
CAPTURE_EXPORT extern const char kBrightness[];
CAPTURE_EXPORT extern const char kContrast[];
CAPTURE_EXPORT extern const char kSaturation[];
CAPTURE_EXPORT extern const char kSharpness[];
CAPTURE_EXPORT extern const char kAutoWhiteBalance[];
CAPTURE_EXPORT extern const char kEventType[];
CAPTURE_EXPORT extern const char kSubscribe[];

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_CONSTANTS_H_
