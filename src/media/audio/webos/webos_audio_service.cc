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

#include "media/audio/webos/webos_audio_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

namespace {

const char kWebOSChromiumAudio[] = "com.webos.chromium.audio";

const char kQuery[] = "query";
const char kInput[] = "input";
const char kOutput[] = "output";
const char kReturnValue[] = "returnValue";
const char kDeviceList[] = "deviceList";
const char kDeviceName[] = "deviceName";
const char kDeviceDetail[] = "deviceNameDetail";
const char kDisplay[] = "display";
const char kConnectedState[] = "connected";
const char kStreamType[] = "streamType";
const char kTrackId[] = "trackId";
const char kVolume[] = "volume";

const char kListSupportedDevices[] = "listSupportedDevices";
const char kRegisterTrack[] = "registerTrack";
const char kUnregisterTrack[] = "unregisterTrack";
const char kSetTrackVolume[] = "setTrackVolume";
const char kSetSourceInputVolume[] = "setSourceInputVolume";

}  // namespace

#define BIND_TO_LUNA_THREAD(function, data)                           \
  base::BindPostTask(luna_response_runner_,                           \
                     base::BindRepeating(function, weak_this_, data), \
                     FROM_HERE)

WebOSAudioService::WebOSAudioService()
    : weak_factory_(this),
      luna_call_thread_("WebOSAudioLunaCallThread"),
      luna_response_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  VLOG(1) << __func__ << " this[" << this << "]";

  luna_call_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::UI, 0));

  weak_this_ = weak_factory_.GetWeakPtr();

  luna_call_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebOSAudioService::EnsureLunaServiceCreated, weak_this_));
}

WebOSAudioService::~WebOSAudioService() {
  VLOG(1) << __func__ << " this[" << this << "]";

  if (luna_call_thread_.IsRunning())
    luna_call_thread_.Stop();
}

bool WebOSAudioService::GetDeviceList(
    bool input,
    std::vector<WebOSAudioService::DeviceEntry>* device_list) {
  base::DictionaryValue register_root;
  register_root.SetKey(kQuery, base::Value((input ? kInput : kOutput)));

  std::string query_payload;
  if (!base::JSONWriter::Write(register_root, &query_payload)) {
    LOG(ERROR) << __func__ << " Failed to write listSupportedDevices payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::AUDIO, kListSupportedDevices),
          query_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value))
    return false;

  base::Value* devices = root_value->FindListPath(kDeviceList);
  if (!devices) {
    LOG(ERROR) << __func__ << " Did not receive camera list: " << response;
    return false;
  }

  auto devices_value =
      base::ListValue::From(base::Value::ToUniquePtrValue(std::move(*devices)));
  for (const auto& value : devices_value->GetList()) {
    const base::Value::Dict* device = value.GetIfDict();
    if (device) {
      auto connected = device->FindBool(kConnectedState);
      if (connected && *connected) {
        std::string display_id;
        const std::string* device_name = device->FindString(kDeviceName);
        const std::string* device_details = device->FindString(kDeviceDetail);
        const std::string* display = device->FindString(kDisplay);
        if (display) {
          std::string id = display->substr(std::string(kDisplay).size(), 1);
          if (!id.empty())
            display_id = std::to_string(std::stoi(id) - 1);
        }
        device_list->push_back(
            DeviceEntry(*device_name, *device_details, display_id));
      }
    }
  }

  return true;
}

std::string WebOSAudioService::RegisterTrack(const std::string& stream_type) {
  VLOG(1) << __func__ << " stream_type: " << stream_type;

  base::DictionaryValue register_root;
  register_root.SetKey(kStreamType, base::Value(stream_type));

  std::string track_payload;
  if (!base::JSONWriter::Write(register_root, &track_payload)) {
    LOG(ERROR) << __func__ << " Failed to write registerTrack payload";
    return std::string();
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::AUDIO, kRegisterTrack),
          track_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return std::string();
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value)) {
    LOG(ERROR) << __func__ << " Wrong return value received";
    return std::string();
  }

  std::string* track_id = root_value->FindStringPath(kTrackId);
  if (!track_id) {
    LOG(ERROR) << __func__ << " Did not receive track id: " << response;
    return std::string();
  }

  return *track_id;
}

bool WebOSAudioService::UnregisterTrack(const std::string& track_id) {
  VLOG(1) << __func__ << " track_id: " << track_id;

  base::DictionaryValue register_root;
  register_root.SetKey(kTrackId, base::Value(track_id));

  std::string track_payload;
  if (!base::JSONWriter::Write(register_root, &track_payload)) {
    LOG(ERROR) << __func__ << " Failed to write unregisterTrack payload";
    return false;
  }

  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::AUDIO, kUnregisterTrack),
          track_payload, nullptr)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  return true;
}

bool WebOSAudioService::SetTrackVolume(const std::string& track_id,
                                       int volume) {
  VLOG(1) << __func__ << " track_id: " << track_id << "volume: " << volume;

  base::DictionaryValue register_root;
  register_root.SetKey(kTrackId, base::Value(track_id));
  register_root.SetKey(kVolume, base::Value(volume));

  std::string volume_payload;
  if (!base::JSONWriter::Write(register_root, &volume_payload)) {
    LOG(ERROR) << __func__ << " Failed to write setTrackVolume payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::AUDIO, kSetTrackVolume),
          volume_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value)) {
    LOG(ERROR) << __func__ << " Wrong return value received";
    return false;
  }

  return true;
}

bool WebOSAudioService::SetSourceInputVolume(const std::string& stream_type,
                                             double volume) {
  VLOG(1) << __func__ << " stream: " << stream_type << "volume: " << volume;

  base::DictionaryValue register_root;
  register_root.SetKey(kStreamType, base::Value(stream_type));
  register_root.SetKey(kVolume, base::Value(volume));

  std::string volume_payload;
  if (!base::JSONWriter::Write(register_root, &volume_payload)) {
    LOG(ERROR) << __func__ << " Failed to write setTrackVolume payload";
    return false;
  }

  std::string response;
  if (!LunaCallInternal(
          base::LunaServiceClient::GetServiceURI(
              base::LunaServiceClient::URIType::AUDIO, kSetSourceInputVolume),
          volume_payload, &response)) {
    LOG(ERROR) << __func__ << " Failed luna service call";
    return false;
  }

  std::unique_ptr<base::DictionaryValue> root_value;
  if (!GetRootDictionary(response, &root_value)) {
    LOG(ERROR) << __func__ << " Wrong return value received";
    return false;
  }

  return true;
}

bool WebOSAudioService::GetRootDictionary(
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

void WebOSAudioService::EnsureLunaServiceCreated() {
  if (luna_service_client_)
    return;

  luna_service_client_.reset(new base::LunaServiceClient(kWebOSChromiumAudio));
  VLOG(1) << __func__ << " luna_service_client_=" << luna_service_client_.get();
}

bool WebOSAudioService::LunaCallInternal(const std::string& uri,
                                         const std::string& param,
                                         std::string* response) {
  VLOG(1) << __func__ << " " << uri << " " << param;

  base::AutoLock auto_lock(audio_service_lock_);

  LunaCbHandle handle(uri, param, response);
  luna_call_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebOSAudioService::LunaCallAsyncInternal,
                                weak_this_, &handle));

  handle.async_done_.Wait();
  return true;
}

void WebOSAudioService::LunaCallAsyncInternal(LunaCbHandle* handle) {
  DCHECK(luna_call_thread_.task_runner()->BelongsToCurrentThread());
  VLOG(1) << __func__ << " " << handle->uri_ << " " << handle->param_;

  EnsureLunaServiceCreated();

  if (!handle->response_) {
    luna_service_client_->CallAsync(handle->uri_, handle->param_);
    handle->async_done_.Signal();
    return;
  }

  luna_service_client_->CallAsync(
      handle->uri_, handle->param_,
      BIND_TO_LUNA_THREAD(&WebOSAudioService::OnLunaCallResponse, handle));
}

void WebOSAudioService::OnLunaCallResponse(LunaCbHandle* handle,
                                           const std::string& response) {
  if (!handle)
    return;

  VLOG(1) << __func__ << " " << handle->uri_ << " " << handle->param_ << " "
          << response;
  if (handle->response_)
    handle->response_->assign(response);

  handle->async_done_.Signal();
}

}  // namespace media
