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

#include "media/neva/fake_url_media_player.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches_neva.h"
#include "media/base/timestamp_constants.h"
#include "media/neva/media_util.h"
#include "neva/logging.h"

#define VIDEO_HEIGHT 1920
#define VIDEO_WEIDTH 1080

namespace media {

FakeURLMediaPlayer::FakeURLMediaPlayer(
    MediaPlayerNevaClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const std::string& app_id)
    : client_(client), main_task_runner_(main_task_runner), app_id_(app_id) {
  NEVA_LOGTF(ERROR);
}

FakeURLMediaPlayer::~FakeURLMediaPlayer() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void FakeURLMediaPlayer::Initialize(const bool is_video,
                                    const double current_time,
                                    const std::string& url,
                                    const std::string& mime_type,
                                    const std::string& referrer,
                                    const std::string& user_agent,
                                    const std::string& cookies,
                                    const std::string& media_option,
                                    const std::string& custom_option) {
  NEVA_DVLOGTF(1) << "app_id: " << app_id_ << " / url: " << url
                  << " / media_option: " << media_option
                  << " / current_time: " << current_time;

  if (!base::StringToDouble(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kFakeUrlMediaDuration),
          &duration_))
    duration_ = 200.0f;

  OnBufferingStateChanged(kHaveMetadata);
  OnBufferingStateChanged(kLoadCompleted);
}

void FakeURLMediaPlayer::Start() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1) << "playback_rate_ : " << playback_rate_;
  media_state_ = Playing;

  SetRate(playback_rate_);

  OnPlaybackStateChanged(true);
  client_->OnTimeUpdate(GetCurrentTime(), base::TimeTicks::Now());
  interpolator_.StartInterpolating();

  if (!time_update_timer_.IsRunning()) {
    time_update_timer_.Start(FROM_HERE,
                             base::Milliseconds(media::kTimeUpdateInterval),
                             this, &FakeURLMediaPlayer::OnTimeUpdateTimerFired);
  }
}

void FakeURLMediaPlayer::Pause() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  interpolator_.StopInterpolating();
  time_update_timer_.Stop();
  OnPlaybackStateChanged(false);
  client_->OnTimeUpdate(GetCurrentTime(), base::TimeTicks::Now());
}

void FakeURLMediaPlayer::Seek(const base::TimeDelta& time) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1) << "seektime : " << time.InSecondsF();

  media_state_to_be_restored_ = media_state_;
  media_state_ = Seeking;

  interpolator_.SetBounds(time, base::Seconds(duration_),
                          base::TimeTicks::Now());
  if (!end_of_stream_)
    OnSeekDone(PIPELINE_OK);
  else
    pending_seek_ = true;
}

void FakeURLMediaPlayer::SetVolume(double volume) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1) << "volume : " << volume;
  volume_ = volume;
}

void FakeURLMediaPlayer::SetPoster(const GURL& poster) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
}

void FakeURLMediaPlayer::SetRate(double rate) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1) << ": " << rate;

  playback_rate_ = rate;
  interpolator_.SetPlaybackRate(playback_rate_);
}

void FakeURLMediaPlayer::SetPreload(MediaPlayerNeva::Preload preload) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  preload_ = preload;
}

bool FakeURLMediaPlayer::IsPreloadable(
    const std::string& content_media_option) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  return true;
}

bool FakeURLMediaPlayer::HasVideo() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(2);
  return true;
}

bool FakeURLMediaPlayer::HasAudio() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(2);
  return true;
}

bool FakeURLMediaPlayer::SelectTrack(const MediaTrackType type,
                                     const std::string& id) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  return true;
}

bool FakeURLMediaPlayer::UsesIntrinsicSize() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return false;
}

std::string FakeURLMediaPlayer::MediaId() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  return "fake_media_id";
}

void FakeURLMediaPlayer::Suspend(SuspendReason reason) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  if (is_suspended_)
    return;

  is_suspended_ = true;
}

void FakeURLMediaPlayer::Resume() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);
  if (!is_suspended_)
    return;

  is_suspended_ = false;
}

bool FakeURLMediaPlayer::RequireMediaResource() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return true;
}

bool FakeURLMediaPlayer::IsRecoverableOnResume() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return true;
}

void FakeURLMediaPlayer::SetDisableAudio(bool disable) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void FakeURLMediaPlayer::OnPlaybackStateChanged(bool playing) {
  NEVA_DVLOGTF(1) << ": " << playing;

  if (playing) {
    media_state_ = Playing;
    client_->OnMediaPlayerPlay();
  } else {
    media_state_ = Paused;
    client_->OnMediaPlayerPause();
  }
}

void FakeURLMediaPlayer::OnStreamEnded() {
  NEVA_DVLOGTF(1);
  time_update_timer_.Stop();
  end_of_stream_ = true;
  pending_seek_ = true;

  client_->OnPlaybackComplete();

  if (pending_seek_) {
    OnSeekDone(PIPELINE_OK);
    pending_seek_ = false;
    end_of_stream_ = false;
  }
}

void FakeURLMediaPlayer::OnSeekDone(PipelineStatus status) {
  NEVA_DVLOGTF(1);

  if (status != PIPELINE_OK) {
    OnError(status);
    return;
  }

  client_->OnSeekComplete(base::Seconds(GetCurrentTime().InSecondsF()));

  media_state_ = media_state_to_be_restored_;
  SetRate(playback_rate_);

  if (media_state_ == Playing)
    OnPlaybackStateChanged(true);
  else
    OnPlaybackStateChanged(false);
}

void FakeURLMediaPlayer::OnError(PipelineStatus error) {
  NEVA_DVLOGTF(1);
  media_state_ = Error;
  client_->OnMediaError(ConvertToMediaError(error));
}

void FakeURLMediaPlayer::OnBufferingStateChanged(
    BufferingState buffering_state) {
  NEVA_DVLOGTF(2) << "state:" << buffering_state;

  // TODO(neva): Ensure following states.
  switch (buffering_state) {
    case kHaveMetadata: {
      gfx::Size videoSize = gfx::Size(VIDEO_HEIGHT, VIDEO_WEIDTH);
      media_state_ = Loading;
      client_->OnMediaMetadataChanged(base::Seconds(duration_), videoSize,
                                      videoSize, true);
    } break;
    case kLoadCompleted:
      client_->OnLoadComplete();
      media_state_ = Ready;
      break;
    case kPreloadCompleted:
      client_->OnLoadComplete();
      media_state_ = Ready;
      break;
    case kPrerollCompleted:
      break;
    case kBufferingStart:
      client_->OnBufferingStateChanged(BUFFERING_HAVE_NOTHING);
      break;
    case kBufferingEnd:
      client_->OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH);
      break;
    case kNetworkStateLoading:
      break;
    case kNetworkStateLoaded:
      break;
  }
}

base::TimeDelta FakeURLMediaPlayer::GetCurrentTime() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(2) << interpolator_.GetInterpolatedTime().InSecondsF();
  return interpolator_.GetInterpolatedTime();
}

void FakeURLMediaPlayer::OnTimeUpdateTimerFired() {
  NEVA_DVLOGTF(2) << ": " << GetCurrentTime().InSecondsF()
                  << " playback_rate_ : " << playback_rate_;
  if (client_)
    client_->OnTimeUpdate(GetCurrentTime(), base::TimeTicks::Now());

  if (GetCurrentTime().InSecondsF() >= duration_) {
    base::TimeDelta bound =
        (playback_rate_ < 0.0f) ? base::TimeDelta() : base::Seconds(duration_);

    interpolator_.SetBounds(bound, bound, base::TimeTicks::Now());
    OnStreamEnded();
  }
}

}  // namespace media
