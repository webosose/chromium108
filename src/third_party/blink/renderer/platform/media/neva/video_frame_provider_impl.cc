// Copyright 2017-2018 LG Electronics, Inc.
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

#include "third_party/blink/public/platform/media/neva/video_frame_provider_impl.h"

#include "base/command_line.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "neva/logging.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace blink {

VideoFrameProviderImpl::VideoFrameProviderImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    : frame_(nullptr),
      client_(nullptr),
      is_suspended_(false),
      natural_video_size_(gfx::Size(1, 1)),
      video_frame_provider_client_(nullptr),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      compositor_task_runner_(compositor_task_runner) {
  NEVA_DVLOGTF(1);
}

VideoFrameProviderImpl::~VideoFrameProviderImpl() {
  NEVA_DVLOGTF(1);
  SetVideoFrameProviderClient(nullptr);
}

void VideoFrameProviderImpl::ActiveRegionChanged(
    const gfx::Rect& active_region) {
  NEVA_DVLOGTF(1) << "active_region:" << active_region.ToString();
  if (active_video_region_ != active_region) {
    active_video_region_ = active_region;
    UpdateVideoFrame();
  }
}

void VideoFrameProviderImpl::UpdateVideoFrame() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DVLOGTF(1);

  current_frame_ = CreateVideoFrame(frame_type_);

  Repaint();
}

scoped_refptr<media::VideoFrame> VideoFrameProviderImpl::CreateVideoFrame(
    FrameType frame_type) {
  NEVA_DVLOGTF(1) << "frame_type:" << frame_type;

  switch (frame_type) {
#if defined(NEVA_VIDEO_HOLE)
    case FrameType::kHole:
      if (!overlay_plane_id_.is_empty()) {
        return media::VideoFrame::CreateVideoHoleFrame(
            overlay_plane_id_, natural_video_size_, base::TimeDelta());
      } else {
        LOG(INFO) << __func__
                  << " overlay_plane_id_ is not set yet. Create black frame.";
        return media::VideoFrame::CreateBlackFrame(natural_video_size_);
      }
#endif
    case FrameType::kBlack:
      return media::VideoFrame::CreateBlackFrame(natural_video_size_);
    default:
      return nullptr;
  }
}

WebMediaPlayerClient* VideoFrameProviderImpl::GetClient() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(client_);
  return client_;
}

void VideoFrameProviderImpl::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  NEVA_DVLOGTF(1);
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;
}

void VideoFrameProviderImpl::PutCurrentFrame() {}

base::TimeDelta VideoFrameProviderImpl::GetPreferredRenderInterval() {
  return base::Microseconds(16666);
}

void VideoFrameProviderImpl::OnContextLost() {
  // current_frame_'s resource in the context has been lost, so current_frame_
  // is not valid any more. current_frame_ should be reset. Now the compositor
  // has no concept of resetting current_frame_, so a black frame is set.
  if (!current_frame_ || (!current_frame_->HasTextures() &&
                          !current_frame_->HasGpuMemoryBuffer())) {
    return;
  }
  scoped_refptr<media::VideoFrame> black_frame =
      media::VideoFrame::CreateBlackFrame(natural_video_size_);
  SetCurrentVideoFrame(std::move(black_frame));
}

scoped_refptr<media::VideoFrame> VideoFrameProviderImpl::GetCurrentFrame() {
  return current_frame_;
}

bool VideoFrameProviderImpl::UpdateCurrentFrame(base::TimeTicks deadline_min,
                                                base::TimeTicks deadline_max) {
  NOTIMPLEMENTED();
  return false;
}

bool VideoFrameProviderImpl::HasCurrentFrame() {
  return current_frame_ != nullptr;
}

void VideoFrameProviderImpl::Repaint() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  CHECK(GetClient());
  GetClient()->Repaint();
}

void VideoFrameProviderImpl::SetNaturalVideoSize(gfx::Size natural_video_size) {
  NEVA_DVLOGTF(1) << natural_video_size.ToString();
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  natural_video_size_ =
      natural_video_size.IsEmpty() ? gfx::Size(1, 1) : natural_video_size;
}

void VideoFrameProviderImpl::SetFrameType(FrameType type) {
  NEVA_DVLOGTF(1) << "type:" << type;
  if (frame_type_ == type)
    return;

  frame_type_ = type;
  UpdateVideoFrame();
}

void VideoFrameProviderImpl::SetOverlayPlaneId(
    const base::UnguessableToken& overlay_plane_id) {
  if (overlay_plane_id_ == overlay_plane_id)
    return;

  overlay_plane_id_ = overlay_plane_id;
  UpdateVideoFrame();
}

void VideoFrameProviderImpl::SetCurrentVideoFrame(
    scoped_refptr<media::VideoFrame> current_frame) {
  current_frame_ = current_frame;
}

base::Lock& VideoFrameProviderImpl::GetLock() {
  return lock_;
}

void VideoFrameProviderImpl::SetWebLocalFrame(WebLocalFrame* frame) {
  frame_ = frame;
}

void VideoFrameProviderImpl::SetWebMediaPlayerClient(
    WebMediaPlayerClient* client) {
  client_ = client;
}

gfx::Rect& VideoFrameProviderImpl::GetActiveVideoRegion() {
  return active_video_region_;
}

gfx::Size VideoFrameProviderImpl::GetNaturalVideoSize() {
  return natural_video_size_;
}

}  // namespace blink
