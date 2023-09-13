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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_WEBRTC_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_WEBRTC_H_

#include "media/base/media_log.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/media/neva/create_video_window_callback.h"
#include "third_party/blink/public/platform/media/neva/video_frame_provider_impl.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/web/modules/mediastream/webmediaplayer_ms.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/platform_window/neva/mojom/video_window.mojom.h"

namespace blink {

using MediaPlatformAPI = media::MediaPlatformAPI;

class WebMediaPlayerImpl;

class BLINK_PLATFORM_EXPORT WebMediaPlayerWebRTC
    : public ui::mojom::VideoWindowClient,
      public WebMediaPlayerMS {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  // |delegate| may be null.
  WebMediaPlayerWebRTC(
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
      media::CreateMediaPlatformAPICB create_media_platform_api_cb);
  ~WebMediaPlayerWebRTC() override;
  WebMediaPlayerWebRTC(const WebMediaPlayerWebRTC&) = delete;
  WebMediaPlayerWebRTC& operator=(const WebMediaPlayerWebRTC&) = delete;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameShown() override;

  // Implements ui::mojom::VideoWindowClient
  void OnVideoWindowCreated(const ui::VideoWindowInfo& info) override;
  void OnVideoWindowDestroyed() override;
  void OnVideoWindowGeometryChanged(const gfx::Rect& rect) override {}
  void OnVideoWindowVisibilityChanged(bool visibility) override {}
  // End of mojom::VideoWindowClient

  void SetRenderMode(WebMediaPlayer::RenderMode mode) override;

  // Overridden from parent WebMediaPlayerMS
  bool HandleVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame) override;

  void TriggerResize() override;
  void OnFirstFrameReceived(media::VideoTransformation video_transform,
                            bool is_opaque) override;
  void OnTransformChanged(media::VideoTransformation video_transform) override;

 private:
  enum class CompositorType {
    kUnknown = -1,
    kWebMediaPlayerMSCompositor = 0,
    kVideoFrameProviderImpl = 1,
  };

  void ResetForDecoderChange();

  void SuspendInternal();
  void ResumeInternal();

  void OnVideoSizeChanged(const gfx::Size& coded_size,
                          const gfx::Size& natural_size);
  void OnResumed();
  void OnSuspended();

  void CreateVideoWindow();
  bool EnsureVideoWindowCreated();
  void ContinuePlayerWithWindowId();

  std::unique_ptr<VideoFrameProviderImpl> video_frame_provider_impl_;
  CompositorType compositor_type_ = CompositorType::kWebMediaPlayerMSCompositor;

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  std::string app_id_;

  // This value is updated by using value from media platform api.
  gfx::Size coded_size_;
  gfx::Size natural_size_;

  scoped_refptr<media::VideoFrame> current_frame_ = nullptr;

  WebMediaPlayer::RenderMode render_mode_ = WebMediaPlayer::RenderModeNone;

  bool is_suspended_ = false;

  bool video_play_started_ = false;

  WebLocalFrame* web_local_frame_ = nullptr;

  // Callback to notify app id, window id and cb status to decoder
  base::RepeatingCallback<void(const std::string&,
                               const std::string&,
                               const base::RepeatingClosure&,
                               const base::RepeatingClosure&,
                               const MediaPlatformAPI::VideoSizeChangedCB&,
                               const MediaPlatformAPI::ActiveRegionCB&)>
      media_player_init_cb_;

  // Callback to notify media player suspend status to decoder
  base::RepeatingCallback<void(bool)> media_player_suspend_cb_;

  CreateVideoWindowCallback create_video_window_callback_;
  absl::optional<ui::VideoWindowInfo> video_window_info_ = absl::nullopt;
  mojo::Remote<ui::mojom::VideoWindow> video_window_remote_;
  mojo::Receiver<ui::mojom::VideoWindowClient> video_window_client_receiver_{
      this};

  base::WeakPtr<WebMediaPlayerWebRTC> weak_ptr_this_;
  base::WeakPtrFactory<WebMediaPlayerWebRTC> weak_factory_this_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_WEBRTC_H_
