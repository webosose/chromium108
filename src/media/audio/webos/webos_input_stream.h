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

#ifndef MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_INPUT_STREAM_H_

#include "media/audio/pulse/pulse_input.h"

namespace media {

class AudioManagerWebOS;

class WebOSAudioInputStream : public PulseAudioInputStream {
 public:
  WebOSAudioInputStream(AudioManagerWebOS* audio_manager,
                        const std::string& device_name,
                        const AudioParameters& params,
                        pa_threaded_mainloop* mainloop,
                        pa_context* context,
                        AudioManager::LogCallback log_callback);

  ~WebOSAudioInputStream() override;

  WebOSAudioInputStream(const WebOSAudioInputStream&) = delete;
  WebOSAudioInputStream& operator=(const WebOSAudioInputStream&) = delete;

  // Implementation of PulseAudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Close() override;
  void SetVolume(double volume) override;
  double GetVolume() override;

 private:
  std::string preferred_device_;
  AudioManagerWebOS* audio_manager_webos_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_INPUT_STREAM_H_
