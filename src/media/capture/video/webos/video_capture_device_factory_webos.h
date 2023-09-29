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

// Implementation of a VideoCaptureDeviceFactoryWebOS class.

#ifndef MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_FACTORY_WEBOS_H_
#define MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_FACTORY_WEBOS_H_

#include "media/capture/video/video_capture_device_factory.h"

namespace base {
class Value;
}  // namespace base

namespace media {

class WebOSCameraService;

class CAPTURE_EXPORT VideoCaptureDeviceFactoryWebOS
    : public VideoCaptureDeviceFactory {
 public:
  explicit VideoCaptureDeviceFactoryWebOS();
  ~VideoCaptureDeviceFactoryWebOS() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

 private:
  VideoCaptureDeviceFactoryWebOS(const VideoCaptureDeviceFactoryWebOS&) =
      delete;
  void operator=(VideoCaptureDeviceFactoryWebOS const&) = delete;

  void GetSupportedFormats(const std::string& device_id,
                           VideoCaptureControlSupport* control,
                           VideoCaptureFormats* supported_formats,
                           base::Value* info);
  void SetSupportedFormat(VideoCaptureFormats& capture_formats,
                          VideoPixelFormat pixel_format,
                          base::Value* supported_resolutions);

  scoped_refptr<WebOSCameraService> camera_service_;

  base::flat_map<std::string, VideoCaptureFormats> supported_formats_cache_;
  base::flat_map<std::string, VideoCaptureControlSupport> controls_cache_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_FACTORY_WEBOS_H_
