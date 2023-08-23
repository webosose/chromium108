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

#include "media/capture/video/webos/webos_camera_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <CameraBuffer.h>

namespace media {

namespace {

const char kWebOSChromiumCamera[] = "com.webos.chromium.camera";

void SigHandlerForCameraService(int signum) {
  VLOG(1) << __func__ << ", signal handler : signum(" << signum << ")";
}

}  // namespace

#define BIND_TO_LUNA_THREAD(function, data)                           \
  base::BindPostTask(luna_response_runner_,                           \
                     base::BindRepeating(function, weak_this_, data), \
                     FROM_HERE)

WebOSCameraService::WebOSCameraService()
    : weak_factory_(this),
      luna_call_thread_("WebOSCameraLunaCallThread"),
      luna_response_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  VLOG(1) << __func__ << " this[" << this << "]";

  luna_call_thread_.Start();

  weak_this_ = weak_factory_.GetWeakPtr();
  luna_service_client_.reset(new base::LunaServiceClient(kWebOSChromiumCamera));

  camera_buffer_.reset(
      new camera::CameraBuffer(camera::CameraBuffer::SHMEM_SYSTEMV));
}

WebOSCameraService::~WebOSCameraService() {
  VLOG(1) << __func__ << " this[" << this << "]";

  if (fault_event_subscribe_key_)
    luna_service_client_->Unsubscribe(fault_event_subscribe_key_);

  if (camera_list_subscribe_key_)
    luna_service_client_->Unsubscribe(camera_list_subscribe_key_);

  if (luna_call_thread_.IsRunning())
    luna_call_thread_.Stop();
}

int WebOSCameraService::Open(base::PlatformThreadId pid,
                             const std::string& device_id,
                             const std::string& mode) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__ << " pid=" << pid << ", device_id=" << device_id
          << ", mode=" << mode;

  signal(SIGUSR1, SigHandlerForCameraService);

  base::DictionaryValue register_root;
  register_root.SetKey(kPid, base::Value(pid));
  register_root.SetKey(kId, base::Value(device_id));
  register_root.SetKey(kMode, base::Value(mode));

  std::string open_payload;
  if (!base::JSONWriter::Write(register_root, &open_payload)) {
    LOG(ERROR) << __func__ << " Failed to write open payload";
    return -1;
  }

  std::string response;
  if (!LunaCallInternal(base::LunaServiceClient::GetServiceURI(
                            base::LunaServiceClient::URIType::CAMERA, kOpen),
                        open_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return -1;

  absl::optional<int> device_handle = root_value->FindIntPath(kHandle);
  if (!device_handle) {
    LOG(ERROR) << __func__ << " Did not receive camera list: " << response;
    return -1;
  }

  return *device_handle;
}

void WebOSCameraService::Close(base::PlatformThreadId pid, int handle) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__ << " pid=" << pid << ", handle=" << handle;

  base::DictionaryValue register_root;
  register_root.SetKey(kPid, base::Value(pid));
  register_root.SetKey(kHandle, base::Value(handle));

  std::string close_payload;
  if (!base::JSONWriter::Write(register_root, &close_payload)) {
    LOG(ERROR) << __func__ << " Failed to write close payload";
    return;
  }

  if (!LunaCallInternal(base::LunaServiceClient::GetServiceURI(
                            base::LunaServiceClient::URIType::CAMERA, kClose),
                        close_payload, nullptr)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
  }
}

bool WebOSCameraService::GetDeviceIds(std::vector<std::string>* device_ids) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__;

  base::DictionaryValue register_root;
  register_root.SetKey(kSubscribe, base::Value(false));

  std::string list_payload;
  if (!base::JSONWriter::Write(register_root, &list_payload)) {
    LOG(ERROR) << __func__ << " Failed to write getCameraList payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kGetCameraList),
          list_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return false;

  base::Value* device_list = root_value->FindListPath(kDeviceList);
  if (!device_list) {
    LOG(ERROR) << __func__ << " Did not receive camera list: " << response;
    return false;
  }

  for (size_t i = 0; i < device_list->GetList().size(); ++i) {
    auto device_entry = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(std::move(device_list->GetList()[i])));
    if (device_entry) {
      std::string* device_id = device_entry->FindStringPath(kId);
      if (device_id && !device_id->empty())
        device_ids->push_back(*device_id);
    }
  }

  return true;
}

std::string WebOSCameraService::GetDeviceName(const std::string& camera_id) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__;

  std::string device_name;
  if (camera_id.empty()) {
    LOG(ERROR) << __func__ << " camera_id is empty";
    return device_name;
  }

  base::DictionaryValue register_root;
  register_root.SetKey(kId, base::Value(camera_id));

  std::string info_payload;
  if (!base::JSONWriter::Write(register_root, &info_payload)) {
    LOG(ERROR) << __func__ << " Failed to write info payload";
    return device_name;
  }

  std::string response;
  if (!LunaCallInternal(base::LunaServiceClient::GetServiceURI(
                            base::LunaServiceClient::URIType::CAMERA, kGetInfo),
                        info_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return device_name;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return device_name;

  auto info_value = root_value->FindDictPath(kInfo);
  if (info_value)
    device_name = *info_value->FindStringPath(kName);

  return device_name;
}

bool WebOSCameraService::GetProperties(int handle, base::Value* properties) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__;

  if (handle < 0) {
    LOG(ERROR) << __func__ << " Invalid camera handle";
    return false;
  }

  base::DictionaryValue register_root;
  register_root.SetKey(kHandle, base::Value(handle));

  std::string properties_payload;
  if (!base::JSONWriter::Write(register_root, &properties_payload)) {
    LOG(ERROR) << __func__ << " Failed to write getProperties payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kGetProperties),
          properties_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return false;

  auto param_value = root_value->FindDictPath(kParams);
  if (properties && param_value)
    *properties = param_value->Clone();

  return true;
}

bool WebOSCameraService::SetProperties(int handle, base::Value* properties) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__;

  if (handle < 0) {
    LOG(ERROR) << __func__ << " Invalid camera handle";
    return false;
  }

  if (properties->is_none()) {
    LOG(ERROR) << __func__ << " Invalid property values";
    return false;
  }

  base::DictionaryValue register_root;
  register_root.SetKey(kHandle, base::Value(handle));
  register_root.SetKey(kParams, properties->Clone());

  std::string properties_payload;
  if (!base::JSONWriter::Write(register_root, &properties_payload)) {
    LOG(ERROR) << __func__ << " Failed to write setProperties payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kSetProperties),
          properties_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  return GetRootDictionary(response, nullptr);
}

bool WebOSCameraService::SetFormat(int handle,
                                   int width,
                                   int height,
                                   const std::string& format,
                                   int fps) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__ << " handle=" << handle << ", width=" << width
          << ", height=" << height << " fps=" << fps;

  if (handle < 0) {
    LOG(ERROR) << __func__ << " Invalid camera handle";
    return false;
  }

  base::DictionaryValue format_value;
  format_value.SetKey(kWidth, base::Value(width));
  format_value.SetKey(kHeight, base::Value(height));
  format_value.SetKey(kFormat, base::Value(format));
  format_value.SetKey(kFps, base::Value(fps));

  base::DictionaryValue format_root;
  format_root.SetKey(kHandle, base::Value(handle));
  format_root.SetKey(kParams, format_value.Clone());

  std::string format_payload;
  if (!base::JSONWriter::Write(format_root, &format_payload)) {
    LOG(ERROR) << __func__ << " Failed to write setFormat payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kSetFormat),
          format_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  return GetRootDictionary(response, nullptr);
}

int WebOSCameraService::StartPreview(int handle) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__ << " handle=" << handle;

  if (handle < 0) {
    LOG(ERROR) << __func__ << " Invalid camera handle";
    return -1;
  }

  base::DictionaryValue param_value;
  param_value.SetKey(kType, base::Value(kSharedMemory));
  param_value.SetKey(kSource, base::Value("0"));

  base::DictionaryValue preview_root;
  preview_root.SetKey(kHandle, base::Value(handle));
  preview_root.SetKey(kParams, param_value.Clone());

  std::string preview_payload;
  if (!base::JSONWriter::Write(preview_root, &preview_payload)) {
    LOG(ERROR) << __func__ << " Failed to write startPreview payload";
    return -1;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kStartPreview),
          preview_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return -1;

  absl::optional<int> shmem_key = root_value->FindIntPath(kKey);
  if (!shmem_key) {
    LOG(ERROR) << __func__ << " Did not receive key value: " << response;
    return -1;
  }

  VLOG(1) << __func__ << " shmem_key=" << *shmem_key;
  return *shmem_key;
}

void WebOSCameraService::StopPreview(int handle) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());

  VLOG(1) << __func__ << " handle=" << handle;

  if (handle < 0) {
    LOG(ERROR) << __func__ << " Invalid camera handle";
    return;
  }

  base::DictionaryValue preview_root;
  preview_root.SetKey(kHandle, base::Value(handle));

  std::string preview_payload;
  if (!base::JSONWriter::Write(preview_root, &preview_payload)) {
    LOG(ERROR) << __func__ << " Failed to write stopPreview payload";
    return;
  }

  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::CAMERA, kStopPreview),
          preview_payload, nullptr)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
  }
}

void WebOSCameraService::SubscribeCameraChange(ResponseCB camera_cb) {
  base::DictionaryValue camera_list_root;
  camera_list_root.SetKey(kSubscribe, base::Value(true));

  std::string camera_list_payload;
  if (!base::JSONWriter::Write(camera_list_root, &camera_list_payload)) {
    LOG(ERROR) << __func__ << " Failed to write getCameraList payload";
    return;
  }

  base::AutoLock auto_lock(camera_service_lock_);
  luna_service_client_->Subscribe(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::CAMERA, kGetCameraList),
      camera_list_payload, &camera_list_subscribe_key_, camera_cb);
}

void WebOSCameraService::SubscribeFaultEvent(ResponseCB event_cb) {
  base::DictionaryValue get_event_root;
  get_event_root.SetKey(kSubscribe, base::Value(true));

  std::string get_event_payload;
  if (!base::JSONWriter::Write(get_event_root, &get_event_payload)) {
    LOG(ERROR) << __func__ << " Failed to write getEventNotification payload";
    return;
  }

  base::AutoLock auto_lock(camera_service_lock_);
  luna_service_client_->Subscribe(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::CAMERA, kGetEventNotification),
      get_event_payload, &fault_event_subscribe_key_, event_cb);
}

bool WebOSCameraService::OpenCameraBuffer(int shmem_key) {
  base::AutoLock auto_lock(camera_service_lock_);
  if (camera_buffer_)
    return camera_buffer_->Open(shmem_key);
  return false;
}

bool WebOSCameraService::ReadCameraBuffer(uint8_t** buffer, int* size) {
  base::AutoLock auto_lock(camera_service_lock_);
  if (camera_buffer_)
    return camera_buffer_->ReadData(buffer, size);
  return false;
}

bool WebOSCameraService::CloseCameraBuffer() {
  base::AutoLock auto_lock(camera_service_lock_);
  if (camera_buffer_)
    return camera_buffer_->Close();
  return false;
}

bool WebOSCameraService::GetRootDictionary(
    const std::string& payload,
    std::unique_ptr<base::DictionaryValue>* root_dictionary) {
  VLOG(1) << __func__ << " payload: " << payload;
  if (payload.empty())
    return false;

  absl::optional<base::Value> root = base::JSONReader::Read(payload);
  if (!root) {
    return false;
  }

  auto root_value = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*root)));
  auto return_value = root_value->FindBoolPath(kReturnValue);
  if (!return_value || !*return_value) {
    LOG(WARNING) << __func__ << " return value false: " << payload;
    return false;
  }

  if (root_dictionary)
    *root_dictionary = std::move(root_value);

  return true;
}

bool WebOSCameraService::LunaCallInternal(const std::string& uri,
                                          const std::string& param,
                                          std::string* response) {
  VLOG(1) << __func__ << " " << uri << " " << param;

  base::AutoLock auto_lock(camera_service_lock_);

  if (!response) {
    luna_service_client_->CallAsync(uri, param);
    return true;
  }

  LunaCbHandle* handle = new LunaCbHandle(uri, response);
  luna_service_client_->CallAsync(
      uri, param,
      BIND_TO_LUNA_THREAD(&WebOSCameraService::OnLunaCallResponse, handle));

  handle->sync_done_.Wait();

  delete handle;
  return true;
}

void WebOSCameraService::OnLunaCallResponse(LunaCbHandle* handle,
                                            const std::string& response) {
  VLOG(1) << __func__ << " response=[" << response << "]";

  if (handle && handle->response_)
    handle->response_->assign(response);

  handle->sync_done_.Signal();
}

}  // namespace media
