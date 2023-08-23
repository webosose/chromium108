// Copyright 2019 LG Electronics, Inc.
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

#include "media/neva/media_platform_api_mock.h"

#include "base/callback_helpers.h"

namespace media {

MediaPlatformAPIMock::MediaPlatformAPIMock(
    const base::RepeatingClosure& resume_done_cb,
    const base::RepeatingClosure& suspend_done_cb)
    : resume_done_cb_(resume_done_cb), suspend_done_cb_(suspend_done_cb) {}

MediaPlatformAPIMock::~MediaPlatformAPIMock() {}

// static
scoped_refptr<MediaPlatformAPI> MediaPlatformAPI::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool video,
    const std::string& app_id,
    const VideoSizeChangedCB& video_size_changed_cb,
    const base::RepeatingClosure& resume_done_cb,
    const base::RepeatingClosure& suspend_done_cb,
    const ActiveRegionCB& active_region_cb,
    const PipelineStatusCB& error_cb) {
  return base::MakeRefCounted<MediaPlatformAPIMock>(resume_done_cb,
                                                    suspend_done_cb);
}

bool MediaPlatformAPI::IsAvailable() {
  return true;
}

void MediaPlatformAPIMock::Initialize(const AudioDecoderConfig& audio_config,
                                      const VideoDecoderConfig& video_config,
                                      const PipelineStatusCB& init_cb) {
  initialized_ = true;
  std::move(init_cb).Run(PIPELINE_OK);
}

bool MediaPlatformAPIMock::Feed(const scoped_refptr<DecoderBuffer>& buffer,
                                FeedType type) {
  return true;
}

void MediaPlatformAPIMock::Seek(base::TimeDelta time) {
  // We cannot expect correct duration in mock implementation. So just provide
  // big enough value.
  interpolator_.SetBounds(time, base::Seconds(100000), base::TimeTicks::Now());
}

void MediaPlatformAPIMock::Suspend(SuspendReason reason) {
  if (!suspend_done_cb_.is_null())
    suspend_done_cb_.Run();
}

void MediaPlatformAPIMock::Resume(base::TimeDelta paused_time,
                                  RestorePlaybackMode restore_playback_mode) {
  if (!resume_done_cb_.is_null())
    resume_done_cb_.Run();
}

void MediaPlatformAPIMock::SetPlaybackRate(float playback_rate) {
  interpolator_.SetPlaybackRate(playback_rate);
  if (playback_rate > 0) {
    interpolator_.StartInterpolating();

    if (!time_update_timer_.IsRunning()) {
      static const int kTimeUpdateInterval = 100;
      time_update_timer_.Start(FROM_HERE,
                               base::Milliseconds(kTimeUpdateInterval), this,
                               &MediaPlatformAPIMock::OnTimeUpdateTimerFired);
    }
  } else {
    interpolator_.StopInterpolating();
    time_update_timer_.Stop();
  }
}

void MediaPlatformAPIMock::SetPlaybackVolume(double volume) {}

bool MediaPlatformAPIMock::AllowedFeedVideo() {
  return initialized_;
}

bool MediaPlatformAPIMock::AllowedFeedAudio() {
  return initialized_;
}

void MediaPlatformAPIMock::Finalize() {}

void MediaPlatformAPIMock::SetKeySystem(const std::string& key_system) {}

bool MediaPlatformAPIMock::IsEOSReceived() {
  return false;
}

void MediaPlatformAPIMock::UpdateVideoConfig(
    const VideoDecoderConfig& video_config) {}

void MediaPlatformAPIMock::SetDisableAudio(bool disable) {}

bool MediaPlatformAPIMock::HaveEnoughData() {
  return true;
}

void MediaPlatformAPIMock::UpdateCurrentTime(const base::TimeDelta& time) {
  if (update_current_time_cb_)
    update_current_time_cb_.Run(time);
  MediaPlatformAPI::UpdateCurrentTime(std::move(time));
}

void MediaPlatformAPIMock::SetUpdateCurrentTimeCb(
    const UpdateCurrentTimeCB& callback) {
  update_current_time_cb_ = callback;
}

void MediaPlatformAPIMock::OnTimeUpdateTimerFired() {
  UpdateCurrentTime(interpolator_.GetInterpolatedTime());
}

}  // namespace media
