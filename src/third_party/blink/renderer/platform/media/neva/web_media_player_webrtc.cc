// Copyright 2020 LG Electronics, Inc.
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

#include "third_party/blink/public/platform/media/neva/web_media_player_webrtc.h"

#include "cc/layers/layer.h"
#include "cc/layers/video_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "media/base/bind_to_current_loop.h"
#include "neva/logging.h"
#include "third_party/blink/public/platform/media/neva/video_frame_provider_impl.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/webmediaplayer_ms_compositor.h"

namespace blink {

#define BIND_TO_RENDER_LOOP_VIDEO_FRAME_PROVIDER(function)   \
  (NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::BindRepeating(             \
       function, (this->video_frame_provider_impl_->AsWeakPtr()))))

#define BIND_TO_RENDER_LOOP(function)                        \
  (NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::BindRepeating(function, weak_ptr_this_)))

namespace {

// Any reasonable size, will be overridden by the decoder anyway.
const gfx::Size kDefaultSize(640, 480);

}  // namespace

WebMediaPlayerWebRTC::WebMediaPlayerWebRTC(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::MediaLog> media_log,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const WebString& sink_id,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    std::unique_ptr<WebVideoFrameSubmitter> submitter,
    bool use_surface_layer,
    CreateVideoWindowCallback create_video_window_callback,
    const WebString& application_id,
    media::CreateMediaPlatformAPICB create_media_platform_api_cb)
    : WebMediaPlayerMS(frame,
                       client,
                       delegate,
                       std::move(media_log),
                       main_render_task_runner,
                       io_task_runner,
                       compositor_task_runner,
                       media_task_runner,
                       worker_task_runner,
                       gpu_factories,
                       sink_id,
                       std::move(create_bridge_callback),
                       std::move(submitter),
                       use_surface_layer),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      app_id_(application_id.Utf8()),
      web_local_frame_(frame),
      create_video_window_callback_(std::move(create_video_window_callback)),
      weak_factory_this_(this) {
  NEVA_VLOGTF(1) << "[" << this << "] "
                 << "delegate_id_: " << delegate_id_;

  weak_ptr_this_ = weak_factory_this_.GetWeakPtr();
}

WebMediaPlayerWebRTC::~WebMediaPlayerWebRTC() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NEVA_VLOGTF(1) << "[" << this << "] "
                 << "delegate_id_: " << delegate_id_;

  if (!media_player_init_cb_.is_null()) {
    ResetForDecoderChange();
    return;
  }

  if (video_frame_provider_impl_) {
    compositor_task_runner_->DeleteSoon(FROM_HERE,
                                        std::move(video_frame_provider_impl_));
  }
}

void WebMediaPlayerWebRTC::OnFrameHidden() {
  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;

  WebMediaPlayerMS::OnFrameHidden();

  SuspendInternal();
}

void WebMediaPlayerWebRTC::OnFrameShown() {
  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;

  WebMediaPlayerMS::OnFrameShown();

  ResumeInternal();
}

void WebMediaPlayerWebRTC::OnVideoWindowCreated(
    const ui::VideoWindowInfo& info) {
  NEVA_VLOGTF(1) << " window_id=" << info.native_window_id;
  video_window_info_ = info;
  video_frame_provider_impl_->SetOverlayPlaneId(info.window_id);
  ContinuePlayerWithWindowId();
}

void WebMediaPlayerWebRTC::OnVideoWindowDestroyed() {
  NEVA_VLOGTF(1);
  video_window_info_ = absl::nullopt;
  video_window_client_receiver_.reset();
}

void WebMediaPlayerWebRTC::SetRenderMode(WebMediaPlayer::RenderMode mode) {
  VLOG(1) << __func__ << " mode[" << render_mode_ << " -> " << mode << "]";

  if (render_mode_ == mode)
    return;

  render_mode_ = mode;

  if (!video_frame_provider_impl_)
    return;

#if defined(NEVA_VIDEO_HOLE)
  video_frame_provider_impl_->SetFrameType(VideoFrameProviderImpl::kHole);
#endif
  video_frame_provider_impl_->UpdateVideoFrame();
}

bool WebMediaPlayerWebRTC::HandleVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  // We handle only first and second video frame which comes via pass through
  // decoder and has is_transparent_frame set to true.
  // First frame is used to decide the compositor type and initializing the
  // video window and second frame is used to start rendering the punch hole.
  // Rest all video frames contains raw local camera frames in I420 format or
  // frames from SW decoder to be rendered using WebMediaPlayerMSCompositor or
  // the transparent video frames from pass through decoder to continue the
  // webrtc pipeline
  CompositorType compositor_type =
      video_frame->metadata().is_transparent_frame
          ? CompositorType::kVideoFrameProviderImpl
          : CompositorType::kWebMediaPlayerMSCompositor;

  if (compositor_type_ != compositor_type) {
    if (compositor_type_ == CompositorType::kVideoFrameProviderImpl)
      ResetForDecoderChange();

    compositor_type_ = compositor_type;
    if (has_first_frame_) {
      absl::optional<media::VideoTransformation> new_transform =
          media::kNoTransformation;
      if (video_frame->metadata().transformation) {
        new_transform = video_frame->metadata().transformation;
        if (new_transform.has_value()) {
          // Recreate video layer because we have fallen back to SW decoding.
          compositor_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&WebMediaPlayerWebRTC::OnTransformChanged,
                             weak_ptr_this_, *new_transform));
        }
      }
    }
  }

  if (compositor_type_ == CompositorType::kWebMediaPlayerMSCompositor) {
    return true;
  }

  VLOG(2) << __func__ << "[" << this << "] "
          << " storage_type=" << video_frame->storage_type()
          << " frame_size=" << video_frame->natural_size().ToString()
          << " delegate_id_: " << delegate_id_;

  // This is first frame from new instance of pass through decoder.
  if (!video_frame->metadata().media_player_suspend_cb.is_null()) {
    if (!media_player_suspend_cb_.is_null())
      media_player_suspend_cb_.Reset();

    media_player_suspend_cb_ = video_frame->metadata().media_player_suspend_cb;
    video_frame->metadata().media_player_suspend_cb.Reset();
  }

  if (!video_frame->metadata().media_player_init_cb.is_null()) {
    if (!media_player_init_cb_.is_null())
      media_player_init_cb_.Reset();

    media_player_init_cb_ = video_frame->metadata().media_player_init_cb;
    video_frame->metadata().media_player_init_cb.Reset();

    // We are holding the video frame, so that we can return later using
    // WebMediaPlayerMS::EnqueueFrame after video window is created.
    // This ensures that OnFirstFrameReceived is called after video window is
    // created, in order to create video layer with video_frame_provider_impl.
    current_frame_ = std::move(video_frame);

    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebMediaPlayerWebRTC::CreateVideoWindow,
                                  weak_ptr_this_));
    return false;
  }

  if (!video_play_started_) {
    // This is second frame from pass through decoder after buffer feed.
    // We can render punch hole now, as buffer feed has started already.
    video_play_started_ = true;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebMediaPlayerWebRTC::SetRenderMode,
                                  weak_ptr_this_, get_client()->RenderMode()));
  }
  return true;
}

void WebMediaPlayerWebRTC::TriggerResize() {
  gfx::Size size(NaturalSize());

  NEVA_VLOGTF(1) << "[" << this << "] size: " << size.ToString();

  if (compositor_type_ != CompositorType::kVideoFrameProviderImpl) {
    WebMediaPlayerMS::TriggerResize();
    return;
  }

  if (!video_frame_provider_impl_)
    return;

  video_frame_provider_impl_->SetNaturalVideoSize(size);
  if (video_window_remote_)
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);

  video_frame_provider_impl_->UpdateVideoFrame();

  if (HasVideo())
    get_client()->SizeChanged();

  client_->DidPlayerSizeChange(NaturalSize());
}

void WebMediaPlayerWebRTC::OnFirstFrameReceived(
    media::VideoTransformation video_transform,
    bool is_opaque) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NEVA_VLOGTF(1) << "[" << this << "] delegate_id_: " << delegate_id_;

  if (compositor_type_ != CompositorType::kVideoFrameProviderImpl) {
    WebMediaPlayerMS::OnFirstFrameReceived(video_transform, is_opaque);
    return;
  }

  if (!video_frame_provider_impl_)
    return;

  has_first_frame_ = true;
  OnTransformChanged(video_transform);
  OnOpacityChanged(is_opaque);

  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);

  TriggerResize();
  ResetCanvasCache();
}

void WebMediaPlayerWebRTC::OnTransformChanged(
    media::VideoTransformation video_transform) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NEVA_VLOGTF(1) << "[" << this << "] delegate_id_: " << delegate_id_;

  if (compositor_type_ != CompositorType::kVideoFrameProviderImpl) {
    WebMediaPlayerMS::OnTransformChanged(video_transform);
    return;
  }

  if (!video_frame_provider_impl_)
    return;

  if (!bridge_) {
    auto new_video_layer = cc::VideoLayer::Create(
        video_frame_provider_impl_.get(), video_transform);
    get_client()->SetCcLayer(new_video_layer.get());
    video_layer_ = std::move(new_video_layer);
  }
}

void WebMediaPlayerWebRTC::ResetForDecoderChange() {
  NEVA_VLOGTF(1);

  if (!media_player_init_cb_.is_null()) {
    media_player_init_cb_.Run(
        std::string(), std::string(), base::RepeatingClosure(),
        base::RepeatingClosure(), MediaPlatformAPI::VideoSizeChangedCB(),
        MediaPlatformAPI::ActiveRegionCB());
  }
  media_player_init_cb_.Reset();
  media_player_suspend_cb_.Reset();

  video_play_started_ = false;

  if (video_frame_provider_impl_) {
    compositor_task_runner_->DeleteSoon(FROM_HERE,
                                        std::move(video_frame_provider_impl_));
    video_frame_provider_impl_ = nullptr;
  }
}

void WebMediaPlayerWebRTC::SuspendInternal() {
  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;

  if (!video_frame_provider_impl_)
    return;

  if (is_suspended_)
    return;

  is_suspended_ = true;

  if (!media_player_suspend_cb_.is_null())
    media_player_suspend_cb_.Run(true);

  if (HasVideo())
    video_frame_provider_impl_->SetFrameType(VideoFrameProviderImpl::kBlack);
}

void WebMediaPlayerWebRTC::ResumeInternal() {
  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;

  if (!video_frame_provider_impl_)
    return;

  if (!is_suspended_)
    return;

  is_suspended_ = false;

  if (!media_player_suspend_cb_.is_null())
    media_player_suspend_cb_.Run(false);
}

void WebMediaPlayerWebRTC::OnVideoSizeChanged(const gfx::Size& coded_size,
                                              const gfx::Size& natural_size) {
  NEVA_VLOGTF(1) << "[" << this << "]"
                 << " natural_size: " << natural_size.ToString()
                 << " delegate_id_: " << delegate_id_;

  if (natural_size_ == natural_size)
    return;

  coded_size_ = coded_size;
  natural_size_ = natural_size;

  if (video_frame_provider_impl_)
    video_frame_provider_impl_->SetNaturalVideoSize(natural_size_);

  if (video_window_remote_)
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
}

void WebMediaPlayerWebRTC::OnResumed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;

  if (!video_frame_provider_impl_)
    return;

  Play();
  client_->ResumePlayback();

  if (HasVideo()) {
    video_frame_provider_impl_->SetFrameType(VideoFrameProviderImpl::kHole);
  }
}

void WebMediaPlayerWebRTC::OnSuspended() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NEVA_VLOGTF(1) << ": delegate_id_: " << delegate_id_;
}

void WebMediaPlayerWebRTC::CreateVideoWindow() {
  NEVA_VLOGTF(1);

  if (!video_frame_provider_impl_) {
    video_frame_provider_impl_ =
        std::make_unique<VideoFrameProviderImpl>(compositor_task_runner_);
    video_frame_provider_impl_->SetWebLocalFrame(web_local_frame_);
    video_frame_provider_impl_->SetWebMediaPlayerClient(client_);
  }

  if (!EnsureVideoWindowCreated())
    return;

  ContinuePlayerWithWindowId();
}

// It returns true if video window is already created and can be continued
// to next step.
bool WebMediaPlayerWebRTC::EnsureVideoWindowCreated() {
  NEVA_VLOGTF(1);

  if (video_window_info_)
    return true;

  // |is_bound()| would be true if we already requested so we need to just wait
  // for response
  if (video_window_client_receiver_.is_bound())
    return false;

  mojo::PendingRemote<ui::mojom::VideoWindowClient> pending_client;
  video_window_client_receiver_.Bind(
      pending_client.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<ui::mojom::VideoWindow> pending_window_remote;
  create_video_window_callback_.Run(
      std::move(pending_client),
      pending_window_remote.InitWithNewPipeAndPassReceiver(),
      ui::VideoWindowParams());
  video_window_remote_.Bind(std::move(pending_window_remote));
  return false;
}

void WebMediaPlayerWebRTC::ContinuePlayerWithWindowId() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NEVA_VLOGTF(1);

  if (!media_player_init_cb_.is_null()) {
    media_player_init_cb_.Run(
        app_id_, video_window_info_->native_window_id,
        BIND_TO_RENDER_LOOP(&WebMediaPlayerWebRTC::OnSuspended),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerWebRTC::OnResumed),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerWebRTC::OnVideoSizeChanged),
        BIND_TO_RENDER_LOOP_VIDEO_FRAME_PROVIDER(
            &VideoFrameProviderImpl::ActiveRegionChanged));
  }

  if (current_frame_) {
    WebMediaPlayerMS::EnqueueFrame(std::move(current_frame_));
    current_frame_ = nullptr;
  }
}

}  // namespace blink
