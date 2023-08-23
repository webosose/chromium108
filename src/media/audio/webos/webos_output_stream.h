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

#ifndef MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_OUTPUT_STREAM_H_

#include "media/audio/pulse/pulse_output.h"

namespace media {

class AudioManagerWebOS;

class WebOSAudioOutputStream : public PulseAudioOutputStream {
 public:
  WebOSAudioOutputStream(const AudioParameters& params,
                         const std::string& device_id,
                         AudioManagerWebOS* audio_manager_webos,
                         AudioManager::LogCallback log_callback);

  ~WebOSAudioOutputStream() override;

  WebOSAudioOutputStream(const WebOSAudioOutputStream&) = delete;
  WebOSAudioOutputStream& operator=(const WebOSAudioOutputStream&) = delete;

  // Implementation of PulseAudioOutputStream.
  bool Open() override;
  void Close() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  std::string track_id_;
  std::string preferred_device_;

  double track_volume_ = 1.0;
  AudioManagerWebOS* audio_manager_webos_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WEBOS_WEBOS_AUDIO_OUTPUT_STREAM_H_
