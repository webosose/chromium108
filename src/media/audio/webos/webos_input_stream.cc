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

#include "media/audio/webos/webos_input_stream.h"

#include "base/logging.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/audio/webos/audio_manager_webos.h"

namespace media {

namespace {
const char kDefaultWebCall[] = "webcall";
}

WebOSAudioInputStream::WebOSAudioInputStream(
    AudioManagerWebOS* audio_manager_webos,
    const std::string& device_name,
    const AudioParameters& params,
    pa_threaded_mainloop* mainloop,
    pa_context* context,
    AudioManager::LogCallback log_callback)
    : PulseAudioInputStream(audio_manager_webos,
                            device_name,
                            params,
                            mainloop,
                            context,
                            std::move(log_callback)),
      audio_manager_webos_(audio_manager_webos) {
  VLOG(1) << __func__ << " this[" << this << "]";

  device_name_ = kDefaultWebCall;
  if (AudioDeviceDescription::IsDefaultDevice(device_name)) {
    std::string display_id = device_name.substr(
        std::string(AudioDeviceDescription::kDefaultDeviceId).size(), 1);
    if (!display_id.empty()) {
      int device_number = std::stoi(display_id);
      if (device_number >= 1) {
        device_name_ = std::string(kDefaultWebCall) + display_id;
      }
    }
  } else {
    preferred_device_ = device_name;
  }
}

WebOSAudioInputStream::~WebOSAudioInputStream() {
  VLOG(1) << __func__ << " this[" << this << "]";
}

AudioInputStream::OpenOutcome WebOSAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());

  pulse::AutoPulseLock auto_lock(pa_mainloop_);
  if (!pulse::CreateInputStream(pa_mainloop_, pa_context_, &handle_, params_,
                                device_name_, &StreamNotifyCallback, this,
                                preferred_device_)) {
    return OpenOutcome::kFailed;
  }

  DCHECK(handle_);
  return OpenOutcome::kSuccess;
}

void WebOSAudioInputStream::Close() {
  PulseAudioInputStream::Close();

  audio_manager_webos_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerBase::ReleaseInputStream,
                                base::Unretained(audio_manager_), this));
}

void WebOSAudioInputStream::SetVolume(double volume) {
  VLOG(1) << __func__ << " volume=" << volume;
  // TODO: Remove this and use AudioManagerWebOS::SetSourceInputVolume
  // When AudioD support track based volume level for input stream as well.
  PulseAudioInputStream::SetVolume(volume);
}

double WebOSAudioInputStream::GetVolume() {
  // TODO: Remove this and directly return volume_ when AudioD
  // start supporting track based volume level for input stream as well.
  return PulseAudioInputStream::GetVolume();
}

}  // namespace media
