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

#include "media/audio/webos/webos_output_stream.h"

#include "base/logging.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/audio/webos/audio_manager_webos.h"

namespace media {

namespace {

const char kDefaultVoipCall[] = "voipcall";

}

WebOSAudioOutputStream::WebOSAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManagerWebOS* audio_manager_webos,
    AudioManager::LogCallback log_callback)
    : PulseAudioOutputStream(params,
                             device_id,
                             audio_manager_webos,
                             std::move(log_callback)),
      audio_manager_webos_(audio_manager_webos) {
  DCHECK(!track_id_.empty());
  VLOG(1) << __func__ << " this[" << this << "]";

  if (params.latency_tag() == AudioLatency::LATENCY_RTC) {
    if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
      if (device_id.size() > 1) {
        std::string display_id = device_id.substr(
            std::string(AudioDeviceDescription::kDefaultDeviceId).size(), 1);
        if (!display_id.empty()) {
          int device_number = std::stoi(display_id);
          if (device_number >= 1) {
            device_id_ = std::string(kDefaultVoipCall) + display_id;
          }
        }
      }
    } else {
      device_id_ = kDefaultVoipCall;
      preferred_device_ = device_id;
    }
  }

  track_id_ = audio_manager_webos_->RegisterTrack(device_id_);
}

WebOSAudioOutputStream::~WebOSAudioOutputStream() {
  VLOG(1) << __func__ << " this[" << this << "]";
  audio_manager_webos_->UnregisterTrack(track_id_);
}

bool WebOSAudioOutputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return pulse::CreateOutputStream(
      &pa_mainloop_, &pa_context_, &pa_stream_, params_, device_id_, track_id_,
      &StreamNotifyCallback, &StreamRequestCallback, this, preferred_device_);
}

void WebOSAudioOutputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  PulseAudioOutputStream::Reset();

  audio_manager_webos_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerBase::ReleaseOutputStream,
                                base::Unretained(manager_), this));
}

void WebOSAudioOutputStream::SetVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());

  track_volume_ = volume;
  if (!track_id_.empty())
    audio_manager_webos_->SetTrackVolume(track_id_, track_volume_);
}

void WebOSAudioOutputStream::GetVolume(double* volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *volume = track_volume_;
}

}  // namespace media
