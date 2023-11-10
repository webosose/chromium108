// Copyright 2018-2019 LG Electronics, Inc.
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

#include "third_party/blink/renderer/platform/media/neva/web_media_player_neva_factory.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/demuxer.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/media/neva/web_media_player_mse.h"
#include "third_party/blink/renderer/platform/media/neva/web_media_player_neva.h"

namespace blink {

bool WebMediaPlayerNevaFactory::Playable(const WebMediaPlayerClient* client) {
  int load_type = client->LoadType();
  switch (load_type) {
    case WebMediaPlayer::kLoadTypeMediaSource:
    case WebMediaPlayer::kLoadTypeBlobURL:
    case WebMediaPlayer::kLoadTypeDataURL:
      return WebMediaPlayerMSE::IsAvailable();
    case WebMediaPlayer::kLoadTypeURL:
      if (WebMediaPlayerNeva::CanSupportMediaType(
              client->ContentMIMEType().Latin1()))
        return true;
      break;
  }

  return false;
}

WebMediaPlayer* WebMediaPlayerNevaFactory::CreateWebMediaPlayerNeva(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
    UrlIndex* url_index,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<media::MediaLog> media_log,
    media::MediaPlayerLoggingID player_id,
    WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
    scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner>
        video_frame_compositor_task_runner,
    WebMediaPlayerBuilder::AdjustAllocatedMemoryCB adjust_allocated_memory_cb,
    WebContentDecryptionModule* initial_cdm,
    media::RequestRoutingTokenCallback request_routing_token_cb,
    base::WeakPtr<media::MediaObserver> media_observer,
    bool enable_instant_source_buffer_gc,
    bool embedded_media_experience_enabled,
    mojo::PendingRemote<media::mojom::MediaMetricsProvider> metrics_provider,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    bool use_surface_layer,
    bool is_background_suspend_enabled,
    bool is_background_video_play_enabled,
    bool is_background_video_track_optimization_supported,
    std::unique_ptr<media::Demuxer> demuxer_override,
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces,
    CreateVideoWindowCallback create_video_window_callback,
    const WebString& application_id,
    bool use_unlimited_media_policy,
    media::CreateMediaPlayerNevaCB create_media_player_neva_cb,
    media::CreateMediaPlatformAPICB create_media_platform_api_cb) {
  switch (client->LoadType()) {
    case WebMediaPlayer::kLoadTypeMediaSource:
    case WebMediaPlayer::kLoadTypeBlobURL:
    case WebMediaPlayer::kLoadTypeDataURL:
      return new WebMediaPlayerMSE(
          frame, client, encrypted_client, delegate,
          std::move(renderer_factory_selector), url_index,
          std::move(compositor), std::move(media_log), player_id,
          std::move(defer_load_cb), std::move(audio_renderer_sink),
          std::move(media_task_runner), std::move(worker_task_runner),
          std::move(compositor_task_runner),
          std::move(video_frame_compositor_task_runner),
          std::move(adjust_allocated_memory_cb), initial_cdm,
          std::move(request_routing_token_cb), std::move(media_observer),
          enable_instant_source_buffer_gc, embedded_media_experience_enabled,
          std::move(metrics_provider), std::move(create_bridge_callback),
          std::move(raster_context_provider), use_surface_layer,
          is_background_suspend_enabled, is_background_video_play_enabled,
          is_background_video_track_optimization_supported,
          std::move(demuxer_override), std::move(remote_interfaces),
          std::move(create_video_window_callback), application_id,
          use_unlimited_media_policy, std::move(create_media_platform_api_cb));
    case WebMediaPlayer::kLoadTypeURL:
      return WebMediaPlayerNeva::Create(
          frame, client, delegate, std::move(media_log),
          std::move(defer_load_cb), std::move(audio_renderer_sink),
          std::move(compositor_task_runner),
          std::move(create_video_window_callback), application_id,
          use_unlimited_media_policy, std::move(create_media_player_neva_cb));
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
