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

#include "media/capture/video/webos/webos_camera_constants.h"

namespace media {

// webOS camera service method names
const char kOpen[] = "open";
const char kClose[] = "close";
const char kGetCameraList[] = "getCameraList";
const char kGetInfo[] = "getInfo";
const char kGetProperties[] = "getProperties";
const char kSetProperties[] = "setProperties";
const char kSetFormat[] = "setFormat";
const char kStartPreview[] = "startPreview";
const char kStopPreview[] = "stopPreview";
const char kGetEventNotification[] = "getEventNotification";

// webOS camera service payload key names
const char kKey[] = "key";
const char kSig[] = "sig";
const char kYUV[] = "YUV";
const char kJPEG[] = "JPEG";
const char kNV12[] = "NV12";
const char kNV21[] = "NV21";
const char kPan[] = "pan";
const char kTilt[] = "tilt";
const char kZoom[] = "zoomAbsolute";
const char kId[] = "id";
const char kReturnValue[] = "returnValue";
const char kDeviceList[] = "deviceList";
const char kInfo[] = "info";
const char kName[] = "name";
const char kMode[] = "mode";
const char kPid[] = "pid";
const char kHandle[] = "handle";
const char kParams[] = "params";
const char kWidth[] = "width";
const char kHeight[] = "height";
const char kFormat[] = "format";
const char kFps[] = "fps";
const char kType[] = "type";
const char kPosixShm[] = "posixshm";
const char kSharedMemory[] = "sharedmemory";
const char kSource[] = "source";
const char kResolution[] = "resolution";
const char kFrequency[] = "frequency";
const char kMax[] = "max";
const char kMin[] = "min";
const char kValue[] = "value";
const char kStep[] = "step";
const char kWhiteBalanceTemperature[] = "whiteBalanceTemperature";
const char kBrightness[] = "brightness";
const char kContrast[] = "contrast";
const char kSaturation[] = "saturation";
const char kSharpness[] = "sharpness";
const char kAutoWhiteBalance[] = "autoWhiteBalance";
const char kEventType[] = "eventType";
const char kSubscribe[] = "subscribe";

}  // namespace media
