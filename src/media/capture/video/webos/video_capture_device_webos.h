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

#ifndef MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_WEBOS_H_
#define MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_WEBOS_H_

#include "media/capture/video/video_capture_device.h"

namespace media {

class WebOSCameraService;
class WebOSCaptureDelegate;

class VideoCaptureDeviceWebOS : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceWebOS(
      scoped_refptr<WebOSCameraService> camera_service,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  ~VideoCaptureDeviceWebOS() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

 protected:
  virtual void SetRotation(int rotation);

 private:
  VideoCaptureDeviceWebOS(const VideoCaptureDeviceWebOS&) = delete;
  void operator=(VideoCaptureDeviceWebOS const&) = delete;

  scoped_refptr<WebOSCameraService> camera_service_;

  const VideoCaptureDeviceDescriptor device_descriptor_;

  int rotation_ = 0;

  std::unique_ptr<WebOSCaptureDelegate> capture_impl_;

  std::vector<base::OnceClosure> photo_requests_queue_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WEBOS_VIDEO_CAPTURE_DEVICE_WEBOS_H_
