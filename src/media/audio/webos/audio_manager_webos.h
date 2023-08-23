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

#ifndef MEDIA_AUDIO_WEBOS_AUDIO_MANAGER_WEBOS_H_
#define MEDIA_AUDIO_WEBOS_AUDIO_MANAGER_WEBOS_H_

#include "base/memory/weak_ptr.h"
#include "base/neva/webos/luna_service_client.h"
#include "media/audio/pulse/audio_manager_pulse.h"

namespace media {

class WebOSAudioService;

class MEDIA_EXPORT AudioManagerWebOS : public AudioManagerPulse {
 public:
  AudioManagerWebOS(std::unique_ptr<AudioThread> audio_thread,
                    AudioLogFactory* audio_log_factory,
                    pa_threaded_mainloop* pa_mainloop,
                    pa_context* pa_context);
  ~AudioManagerWebOS() override;

  AudioManagerWebOS(const AudioManagerWebOS&) = delete;
  AudioManagerWebOS& operator=(const AudioManagerWebOS&) = delete;

  // Implementation of AudioManager.
  const char* GetName() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  base::SingleThreadTaskRunner* GetTaskRunner() const override;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() const override;
  void ShutdownOnAudioThread() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  std::string RegisterTrack(const std::string& stream_type);
  void UnregisterTrack(const std::string& track_id);

  void SetTrackVolume(const std::string& track_id, double volume);
  void SetSourceInputVolume(const std::string& stream_type, double volume);

 private:
  AudioOutputStream* MakeWebOSOutputStream(const AudioParameters& params,
                                           const std::string& device_id,
                                           LogCallback log_callback);
  AudioInputStream* MakeWebOSInputStream(const AudioParameters& params,
                                         const std::string& device_id,
                                         LogCallback log_callback);

  void GetAudioDeviceNames(bool input_device, AudioDeviceNames* device_names);

  scoped_refptr<WebOSAudioService> audio_service_;

  AudioDeviceNames* device_names_;

  // For posting tasks from main thread to |media_task_runner_|.
  base::WeakPtr<AudioManagerWebOS> weak_this_;
  base::WeakPtrFactory<AudioManagerWebOS> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WEBOS_AUDIO_MANAGER_WEBOS_H_
