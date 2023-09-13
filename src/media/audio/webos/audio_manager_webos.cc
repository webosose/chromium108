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

#include "media/audio/webos/audio_manager_webos.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/pulse/pulse_output.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/audio/webos/webos_audio_service.h"
#include "media/audio/webos/webos_input_stream.h"
#include "media/audio/webos/webos_output_stream.h"
#include "media/base/audio_parameters.h"

namespace media {

AudioManagerWebOS::AudioManagerWebOS(std::unique_ptr<AudioThread> audio_thread,
                                     AudioLogFactory* audio_log_factory,
                                     pa_threaded_mainloop* pa_mainloop,
                                     pa_context* pa_context)
    : AudioManagerPulse(std::move(audio_thread),
                        audio_log_factory,
                        pa_mainloop,
                        pa_context),
      audio_service_(base::MakeRefCounted<WebOSAudioService>()),
      weak_factory_(this) {
  VLOG(1) << __func__ << " this[" << this << "]";
  weak_this_ = weak_factory_.GetWeakPtr();
}

AudioManagerWebOS::~AudioManagerWebOS() {
  VLOG(1) << __func__ << " this[" << this << "]";
}

const char* AudioManagerWebOS::GetName() {
  return "WebOSPulseAudio";
}

AudioOutputStream* AudioManagerWebOS::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeWebOSOutputStream(params, AudioDeviceDescription::kDefaultDeviceId,
                               log_callback);
}

AudioOutputStream* AudioManagerWebOS::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeWebOSOutputStream(
      params,
      device_id.empty() ? AudioDeviceDescription::kDefaultDeviceId : device_id,
      log_callback);
}

AudioInputStream* AudioManagerWebOS::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeWebOSInputStream(params, device_id, log_callback);
}

AudioInputStream* AudioManagerWebOS::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeWebOSInputStream(params, device_id, log_callback);
}

void AudioManagerWebOS::GetAudioDeviceNames(bool input_device,
                                            AudioDeviceNames* device_names) {
  VLOG(1) << __func__ << " type= " << (input_device ? "input" : "output");

  std::vector<WebOSAudioService::DeviceEntry> device_list;
  audio_service_->GetDeviceList(input_device, &device_list);

  for (auto device : device_list) {
    device_names->push_back(
        AudioDeviceName(device.device_details, device.device_name));
    device_names->back().display_id = device.display_id;
  }

  // Prepend the default device if the list is not empty.
  if (!device_names->empty())
    device_names->push_front(AudioDeviceName::CreateDefault());
}

std::string AudioManagerWebOS::RegisterTrack(const std::string& stream_type) {
  return audio_service_->RegisterTrack(stream_type);
}

void AudioManagerWebOS::UnregisterTrack(const std::string& track_id) {
  audio_service_->UnregisterTrack(track_id);
}

void AudioManagerWebOS::SetTrackVolume(const std::string& track_id,
                                       double volume) {
  VLOG(1) << __func__ << " track_id=" << track_id << " volume=" << volume;
  audio_service_->SetTrackVolume(track_id, static_cast<int>(volume * 100));
}

void AudioManagerWebOS::SetSourceInputVolume(const std::string& stream_type,
                                             double volume) {
  VLOG(1) << __func__ << " stream_type=" << stream_type << " volume=" << volume;
  audio_service_->SetSourceInputVolume(stream_type,
                                       static_cast<int>(volume * 100));
}

AudioOutputStream* AudioManagerWebOS::MakeWebOSOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    LogCallback log_callback) {
  DCHECK(!device_id.empty());
  std::string real_device_id = device_id;
  if ((real_device_id.empty() ||
       real_device_id == AudioDeviceDescription::kDefaultDeviceId) &&
      !params.device_id().empty()) {
    real_device_id = params.device_id();
  }

  VLOG(1) << __func__ << " device_id[" << device_id << "], params.device_id["
          << params.device_id() << "]";
  return new WebOSAudioOutputStream(params, real_device_id, this,
                                    std::move(log_callback));
}

AudioInputStream* AudioManagerWebOS::MakeWebOSInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    LogCallback log_callback) {
  VLOG(1) << __func__ << " device_id[" << device_id << "], params.device_id["
          << params.device_id() << "]";

  return new WebOSAudioInputStream(this, device_id, params, input_mainloop_,
                                   input_context_, std::move(log_callback));
}

}  // namespace media
