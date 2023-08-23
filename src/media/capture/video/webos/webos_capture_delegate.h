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

#ifndef MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAPTURE_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAPTURE_DELEGATE_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "media/capture/video/video_capture_device.h"

namespace base {
class Location;
class Value;
}  // namespace base

namespace media {

class WebOSCameraService;

// Class doing the actual camera capture using webOS camera service.
// Created on the owner's thread, otherwise living, operating and destroyed
// on |camera_task_runner_|.
class CAPTURE_EXPORT WebOSCaptureDelegate final {
 public:
  WebOSCaptureDelegate(
      scoped_refptr<WebOSCameraService> camera_service,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      const scoped_refptr<base::SingleThreadTaskRunner>& camera_task_runner,
      int power_line_frequency,
      int rotation);
  ~WebOSCaptureDelegate();

  // Forward-to versions of VideoCaptureDevice virtual methods.
  void AllocateAndStart(base::PlatformThreadId pid,
                        int width,
                        int height,
                        float frame_rate,
                        std::unique_ptr<VideoCaptureDevice::Client> client);
  void StopAndDeAllocate(base::PlatformThreadId pid);

  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);

  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);
  void SetRotation(int rotation);

  base::WeakPtr<WebOSCaptureDelegate> GetWeakPtr();

 private:
  WebOSCaptureDelegate(const WebOSCaptureDelegate&) = delete;
  void operator=(WebOSCaptureDelegate const&) = delete;

  void DoCapture();

  void SetErrorState(VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);
  VideoPixelFormat GetPixelFormat(int width, int height, int frame_rate);
  bool IsResolutionSupported(const std::string& resolution,
                             base::Value* resolution_list);

  void OnCameraListUpdated(const std::string& response);
  void OnFaultEventOccured(const std::string& response);

  scoped_refptr<WebOSCameraService> camera_service_;

  const scoped_refptr<base::SingleThreadTaskRunner> camera_task_runner_;
  const VideoCaptureDeviceDescriptor device_descriptor_;
  const int power_line_frequency_;

  VideoCaptureFormat capture_format_;
  base::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callbacks_;

  int shmem_key_ = -1;
  int camera_handle_ = -1;

  int rotation_ = 0;
  bool is_capturing_ = false;
  base::TimeTicks first_ref_time_;

  base::TimeTicks start_time_ = base::TimeTicks::Now();
  uint32_t frames_per_sec_ = 0;

  std::unique_ptr<VideoCaptureDevice::Client> client_;

  base::WeakPtr<WebOSCaptureDelegate> weak_this_;
  base::WeakPtrFactory<WebOSCaptureDelegate> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAPTURE_DELEGATE_H_
