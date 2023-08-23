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

#include "media/capture/video/webos/video_capture_device_factory_webos.h"

#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/webos/video_capture_device_webos.h"
#include "media/capture/video/webos/webos_camera_service.h"

namespace media {

namespace {

bool IsControlSupported(base::Value* property) {
  if (!property)
    return false;

  absl::optional<double> max, min, current, step;
  max = property->FindDoublePath(kMax);
  min = property->FindDoublePath(kMin);
  current = property->FindDoublePath(kValue);
  step = property->FindDoublePath(kStep);

  return (max && min && current && step);
}

}  // namespace

VideoCaptureDeviceFactoryWebOS::VideoCaptureDeviceFactoryWebOS()
    : camera_service_(base::MakeRefCounted<WebOSCameraService>()) {
  VLOG(1) << __func__ << " this[" << this << "]";
}

VideoCaptureDeviceFactoryWebOS::~VideoCaptureDeviceFactoryWebOS() {
  VLOG(1) << __func__ << " this[" << this << "]";
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryWebOS::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto video_capture_device = std::make_unique<VideoCaptureDeviceWebOS>(
      camera_service_, device_descriptor);
  return VideoCaptureErrorOrDevice(std::move(video_capture_device));
}

void VideoCaptureDeviceFactoryWebOS::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  camera_service_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceFactoryWebOS::GetDevicesInfoAsync,
                     base::Unretained(this),
                     media::BindToCurrentLoop(std::move(callback))));
}

void VideoCaptureDeviceFactoryWebOS::GetDevicesInfoAsync(
    GetDevicesInfoCallback callback) {
  std::vector<VideoCaptureDeviceInfo> devices_info;

  std::vector<std::string> device_ids;
  camera_service_->GetDeviceIds(&device_ids);
  if (device_ids.empty()) {
    LOG(ERROR) << __func__ << " Empty list of device ids";
    std::move(callback).Run(std::move(devices_info));
    return;
  }

  for (size_t i = 0; i < device_ids.size(); i++) {
    std::string device_id = device_ids[i];
    std::string device_name = camera_service_->GetDeviceName(device_id);
    VLOG(1) << __func__ << " id=" << device_id << " name=" << device_name;

    VideoCaptureFormats supported_formats;
    VideoCaptureControlSupport supported_control;

    bool found_control = false;
    auto control_it = controls_cache_.find(device_id);
    if (control_it != controls_cache_.end()) {
      supported_control = control_it->second;
      found_control = true;
    }

    bool found_formats = false;
    auto it = supported_formats_cache_.find(device_id);
    if (it != supported_formats_cache_.end()) {
      supported_formats = it->second;
      found_formats = true;
    }

    if (!found_control || !found_formats) {
      GetSupportedFormats(device_id,
                          found_control ? nullptr : &supported_control,
                          found_formats ? nullptr : &supported_formats);
    }

    if (supported_formats.empty()) {
      LOG(WARNING) << __func__ << " No supported formats for: " << device_id;
      continue;
    }

    devices_info.emplace_back(VideoCaptureDeviceDescriptor(
        device_name, device_id, device_id, VideoCaptureApi::UNKNOWN,
        supported_control));

    devices_info.back().supported_formats = std::move(supported_formats);
  }

  // Remove old entries from |supported_formats_cache_| if necessary.
  if (supported_formats_cache_.size() > devices_info.size()) {
    base::EraseIf(supported_formats_cache_, [&devices_info](const auto& entry) {
      return base::ranges::none_of(
          devices_info, [&entry](const VideoCaptureDeviceInfo& info) {
            return entry.first == info.descriptor.device_id;
          });
    });
  }

  // Remove old entries from |controls_cache_| if necessary.
  if (controls_cache_.size() > devices_info.size()) {
    base::EraseIf(controls_cache_, [&devices_info](const auto& entry) {
      return base::ranges::none_of(
          devices_info, [&entry](const VideoCaptureDeviceInfo& info) {
            return entry.first == info.descriptor.device_id;
          });
    });
  }

  VLOG(1) << __func__ << " device info size=" << devices_info.size();
  std::move(callback).Run(std::move(devices_info));
}

void VideoCaptureDeviceFactoryWebOS::GetSupportedFormats(
    const std::string& device_id,
    VideoCaptureControlSupport* supported_control,
    VideoCaptureFormats* supported_formats) {
  if (!supported_control && !supported_formats)
    return;

  base::PlatformThreadId pid = camera_service_->GetThreadId();
  int camera_handle = camera_service_->Open(pid, device_id, "secondary");
  if (camera_handle <= 0) {
    LOG(ERROR) << __func__ << " Failed opening device: " << device_id;
    return;
  }

  base::Value properties;
  if (!camera_service_->GetProperties(camera_handle, &properties) ||
      !properties.is_dict()) {
    LOG(ERROR) << __func__ << " Failed getting properties for: " << device_id;
    return;
  }

  if (supported_control) {
    VideoCaptureControlSupport control_support;
    control_support.pan = IsControlSupported(properties.FindDictPath(kPan));
    control_support.tilt = IsControlSupported(properties.FindDictPath(kTilt));
    control_support.zoom = IsControlSupported(properties.FindDictPath(kZoom));
    controls_cache_.emplace(device_id, control_support);
    *supported_control = control_support;
  }

  if (supported_formats) {
    base::Value* formats = properties.FindDictPath(kResolution);
    if (formats) {
      VideoCaptureFormats capture_formats;
      SetSupportedFormat(capture_formats, PIXEL_FORMAT_YUY2,
                         formats->FindListPath(kYUV));
      SetSupportedFormat(capture_formats, PIXEL_FORMAT_MJPEG,
                         formats->FindListPath(kJPEG));
      SetSupportedFormat(capture_formats, PIXEL_FORMAT_NV12,
                         formats->FindListPath(kNV12));
      SetSupportedFormat(capture_formats, PIXEL_FORMAT_NV21,
                         formats->FindListPath(kNV21));

      supported_formats_cache_.emplace(device_id, capture_formats);
      *supported_formats = capture_formats;
    }
  }

  VLOG(1) << __func__ << " supported_formats=" << supported_formats->size();

  camera_service_->Close(pid, camera_handle);
}

void VideoCaptureDeviceFactoryWebOS::SetSupportedFormat(
    VideoCaptureFormats& capture_formats,
    VideoPixelFormat pixel_format,
    base::Value* supported_resolutions) {
  if (!supported_resolutions) {
    VLOG(1) << __func__ << " Empty resolution for: " << pixel_format;
    return;
  }

  for (size_t i = 0; i < supported_resolutions->GetList().size(); ++i) {
    std::string resolution = supported_resolutions->GetList()[i].GetString();
    VLOG(2) << __func__ << " " << pixel_format << " [" << resolution << "]";
    if (!resolution.empty()) {
      VideoCaptureFormat capture_format;
      std::string width, height, freq;
      std::stringstream string_stream(resolution);
      std::getline(string_stream, width, ',');
      std::getline(string_stream, height, ',');
      std::getline(string_stream, freq);
      capture_format.pixel_format = pixel_format;
      capture_format.frame_size.SetSize(std::stoi(width), std::stoi(height));
      capture_format.frame_rate = std::stoi(freq);
      capture_formats.push_back(capture_format);
    }
  }
}

}  // namespace media
