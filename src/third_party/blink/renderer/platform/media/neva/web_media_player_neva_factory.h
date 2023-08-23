// Copyright 2018-2020 LG Electronics, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_FACTORY_H_

#include "media/base/renderer_factory_selector.h"
#include "third_party/blink/public/platform/media/neva/create_video_window_callback.h"
#include "third_party/blink/public/platform/media/url_index.h"
#include "third_party/blink/public/platform/media/video_frame_compositor.h"
#include "third_party/blink/public/platform/media/web_media_player_builder.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class WebMediaPlayerDelegate;

class BLINK_PLATFORM_EXPORT WebMediaPlayerNevaFactory {
 public:
  // True if WebMediaPlayerNevaFactory has a WebMediaPlayerNeva that
  // can play given client.
  static bool Playable(const WebMediaPlayerClient* client);

  static WebMediaPlayer* CreateWebMediaPlayerNeva(
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
      const WebString& file_security_origin,
      bool use_unlimited_media_policy,
      media::CreateMediaPlayerNevaCB create_media_player_neva_cb,
      media::CreateMediaPlatformAPICB create_media_platform_api_cb);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_FACTORY_H_
