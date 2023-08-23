// Copyright 2015 LG Electronics, Inc.
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

#include "third_party/blink/renderer/platform/media/neva/web_media_player_mse.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "cc/layers/video_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer.h"
#include "media/base/renderer_factory_selector.h"
#include "media/neva/media_constants.h"
#include "media/neva/media_platform_api.h"
#include "media/neva/media_preferences.h"
#include "neva/logging.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/renderer/platform/media/power_status_helper.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

#define BIND_TO_RENDER_LOOP_VIDEO_FRAME_PROVIDER(function)   \
  (NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::BindRepeating(             \
       function, (this->video_frame_provider_->AsWeakPtr()))))

#define BIND_TO_RENDER_LOOP(function)                        \
  (NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(                                 \
       base::BindRepeating(function, weak_this_for_mse_)))

WebMediaPlayerMSE::WebMediaPlayerMSE(
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
    bool is_background_video_playback_enabled,
    bool is_background_video_track_optimization_supported,
    std::unique_ptr<media::Demuxer> demuxer_override,
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces,
    CreateVideoWindowCallback create_video_window_callback,
    const WebString& application_id,
    bool use_unlimited_media_policy,
    media::CreateMediaPlatformAPICB create_media_platform_api_cb)
    : WebMediaPlayerImpl(frame,
                         client,
                         encrypted_client,
                         delegate,
                         std::move(renderer_factory_selector),
                         url_index,
                         std::move(compositor),
                         std::move(media_log),
                         player_id,
                         std::move(defer_load_cb),
                         std::move(audio_renderer_sink),
                         std::move(media_task_runner),
                         std::move(worker_task_runner),
                         std::move(compositor_task_runner),
                         std::move(video_frame_compositor_task_runner),
                         std::move(adjust_allocated_memory_cb),
                         initial_cdm,
                         std::move(request_routing_token_cb),
                         std::move(media_observer),
                         enable_instant_source_buffer_gc,
                         embedded_media_experience_enabled,
                         std::move(metrics_provider),
                         std::move(create_bridge_callback),
                         std::move(raster_context_provider),
                         use_surface_layer,
                         is_background_suspend_enabled,
                         is_background_video_playback_enabled,
                         is_background_video_track_optimization_supported,
                         std::move(demuxer_override),
                         std::move(remote_interfaces)),
      app_id_(application_id.Utf8()),
      require_media_resource_(!use_unlimited_media_policy),
      create_video_window_callback_(std::move(create_video_window_callback)) {
  weak_this_for_mse_ = weak_factory_for_mse_.GetWeakPtr();

  // Use the null sink for our MSE player
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      new media::NullAudioSink(media_task_runner_), media_log_.get());

  video_frame_provider_ =
      std::make_unique<VideoFrameProviderImpl>(vfc_task_runner_);
  video_frame_provider_->SetWebLocalFrame(frame);
  video_frame_provider_->SetWebMediaPlayerClient(client);

  // Create MediaPlatformAPI
  auto create_media_platform_api_callback =
      std::move(create_media_platform_api_cb);

  if (create_media_platform_api_callback) {
    media_platform_api_ = create_media_platform_api_callback.Run(
        media_task_runner_, client_->IsVideo(), app_id_,
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnVideoSizeChanged),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnResumed),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnSuspended),
        BIND_TO_RENDER_LOOP_VIDEO_FRAME_PROVIDER(
            &VideoFrameProviderImpl::ActiveRegionChanged),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnError));
  } else {
    media_platform_api_ = media::MediaPlatformAPI::Create(
        media_task_runner_, client_->IsVideo(), app_id_,
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnVideoSizeChanged),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnResumed),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnSuspended),
        BIND_TO_RENDER_LOOP_VIDEO_FRAME_PROVIDER(
            &VideoFrameProviderImpl::ActiveRegionChanged),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerMSE::OnError));
  }

  media_platform_api_->SetMediaPreferences(
      media::MediaPreferences::Get()->GetRawMediaPreferences());
  media_platform_api_->SetMediaCodecCapabilities(
      media::MediaPreferences::Get()->GetMediaCodecCapabilities());

  absl::optional<bool> is_audio_disabled = client_->IsAudioDisabled();
  if (is_audio_disabled.has_value())
    SetDisableAudio(*is_audio_disabled);

  renderer_factory_selector_->GetCurrentFactory()->SetMediaPlatformAPI(
      media_platform_api_);

  SetRenderMode(client_->RenderMode());

  // Disable background video optimization.
  // This feature makes a trouble when it is combined with neva's
  // suspend/resume logic. This feature prevents decoding by disabling
  // demuxer stream. But it is not recovered after shown so that pipeline
  // became freezing.
  is_background_video_optimization_enabled_ = false;
}

WebMediaPlayerMSE::~WebMediaPlayerMSE() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (video_layer_)
    video_layer_->StopUsingProvider();

  vfc_task_runner_->DeleteSoon(FROM_HERE, std::move(video_frame_provider_));

  if (media_platform_api_.get())
    media_platform_api_->Finalize();
}

// static
bool WebMediaPlayerMSE::IsAvailable() {
  return media::MediaPlatformAPI::IsAvailable();
}

WebMediaPlayer::LoadTiming WebMediaPlayerMSE::Load(
    LoadType load_type,
    const WebMediaPlayerSource& source,
    CorsMode cors_mode,
    bool is_cache_disabled) {
  NEVA_DCHECK(source.IsURL());
  NEVA_VLOGTF(1);

  is_loading_ = true;
  pending_load_type_ = load_type;
  pending_source_ = WebMediaPlayerSource(source.GetAsURL());
  pending_cors_mode_ = cors_mode;
  pending_is_cache_disabled_ = is_cache_disabled;

  client_->DidMediaActivationNeeded();

  return LoadTiming::kDeferred;
}

void WebMediaPlayerMSE::Play() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!has_activation_permit_) {
    status_on_suspended_ = PlayingStatus;
    pending_request_.pending_play_ = true;
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_play_ = absl::nullopt;

  WebMediaPlayerImpl::Play();
}

void WebMediaPlayerMSE::Pause() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  if (is_suspended_) {
    status_on_suspended_ = PausedStatus;
    return;
  }
  WebMediaPlayerImpl::Pause();
}

void WebMediaPlayerMSE::Seek(double seconds) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!has_activation_permit_) {
    pending_request_.pending_seek_time_ = base::Seconds(seconds);
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_seek_time_ = absl::nullopt;

  WebMediaPlayerImpl::Seek(seconds);
}

void WebMediaPlayerMSE::SetRate(double rate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!has_activation_permit_) {
    pending_request_.pending_rate_ = rate;
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_rate_ = absl::nullopt;

  WebMediaPlayerImpl::SetRate(rate);
}

void WebMediaPlayerMSE::SetVolume(double volume) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  WebMediaPlayerImpl::SetVolume(volume);
}

void WebMediaPlayerMSE::SetVolumeMultiplier(double multiplier) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva, sync-to-91): Need to be investigated.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebMediaPlayerMSE::OnVideoSizeChanged(const gfx::Size& coded_size,
                                           const gfx::Size& natural_size) {
  NEVA_LOGTF(INFO) << "coded_size: " << coded_size.ToString()
                   << " / natural_size: " << natural_size.ToString();
  coded_size_ = coded_size;
  natural_size_ = natural_size;
  if (video_window_remote_)
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
}

void WebMediaPlayerMSE::OnError(media::PipelineStatus status) {
  NEVA_LOGTF(INFO) << "is_loading_ : " << (is_loading_ ? "true" : "false")
                   << " status : " << PipelineStatusToString(status);
  if (is_loading_) {
    is_loading_ = false;
    client_->DidMediaActivated();
  }
  WebMediaPlayerImpl::OnError(status);
}

void WebMediaPlayerMSE::OnMetadata(const media::PipelineMetadata& metadata) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (is_loading_) {
    is_loading_ = false;
    client_->DidMediaActivated();
  }

  // Cache the |time_to_metadata_| to use for adjusting the TimeToFirstFrame and
  // TimeToPlayReady metrics later if we end up doing a suspended startup.
  time_to_metadata_ = base::TimeTicks::Now() - load_start_time_;
  media_metrics_provider_->SetTimeToMetadata(time_to_metadata_);
  RecordTimingUMA("Media.TimeToMetadata", time_to_metadata_);

  MaybeSetContainerNameForMetrics();

  pipeline_metadata_ = metadata;
  if (power_status_helper_)
    power_status_helper_->SetMetadata(metadata);

  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoRotation",
      metadata.video_decoder_config.video_transformation().rotation,
      media::VIDEO_ROTATION_MAX + 1);

  if (HasAudio()) {
    media_metrics_provider_->SetHasAudio(metadata.audio_decoder_config.codec());
    RecordEncryptionScheme("Audio",
                           metadata.audio_decoder_config.encryption_scheme());
  }

  if (HasVideo()) {
    media_metrics_provider_->SetHasVideo(metadata.video_decoder_config.codec());
    RecordEncryptionScheme("Video",
                           metadata.video_decoder_config.encryption_scheme());

    // TODO(neva): In here, we don't use natural size from platform api.
    // We need to ensure that it is really fine.

    if (pipeline_metadata_.video_decoder_config.video_transformation()
                .rotation == media::VIDEO_ROTATION_90 ||
        pipeline_metadata_.video_decoder_config.video_transformation()
                .rotation == media::VIDEO_ROTATION_270) {
      gfx::Size size = pipeline_metadata_.natural_size;
      pipeline_metadata_.natural_size = gfx::Size(size.height(), size.width());
    }

    // TODO(neva): We don't support media::kUseSurfaceLayerForVideo feature.
    CHECK(!surface_layer_for_video_enabled_);

    NEVA_DCHECK(!video_layer_);

    // Assume that first frame is received
    if (!has_first_frame_)
      WebMediaPlayerImpl::OnFirstFrame(base::TimeTicks::Now());

    video_frame_provider_->SetNaturalVideoSize(pipeline_metadata_.natural_size);
    video_frame_provider_->UpdateVideoFrame();

    video_layer_ = cc::VideoLayer::Create(
        video_frame_provider_.get(),
        pipeline_metadata_.video_decoder_config.video_transformation()
            .rotation);
    video_layer_->SetContentsOpaque(opaque_);
    client_->SetCcLayer(video_layer_.get());
  }

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  // TODO(dalecurtis): Don't create these until kReadyStateHaveFutureData; when
  // we create them early we just increase the chances of needing to throw them
  // away unnecessarily.
  CreateWatchTimeReporter();
  CreateVideoDecodeStatsReporter();

  // SetReadyState() may trigger all sorts of calls into this class (e.g.,
  // Play(), Pause(), etc) so do it last to avoid unexpected states during the
  // calls. An exception to this is UpdatePlayState(), which is safe to call and
  // needs to use the new ReadyState in its calculations.
  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  UpdatePlayState();
}

void WebMediaPlayerMSE::OnVideoWindowCreated(const ui::VideoWindowInfo& info) {
  video_window_info_ = info;
  video_frame_provider_->SetOverlayPlaneId(info.window_id);
  media_platform_api_->SetMediaLayerId(info.native_window_id);
  if (!coded_size_.IsEmpty() || !natural_size_.IsEmpty())
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
  ContinuePlayerWithWindowId();
}

void WebMediaPlayerMSE::OnVideoWindowDestroyed() {
  video_window_info_ = absl::nullopt;
  video_window_client_receiver_.reset();
}

void WebMediaPlayerMSE::OnVideoWindowGeometryChanged(const gfx::Rect& rect) {}

void WebMediaPlayerMSE::OnVideoWindowVisibilityChanged(bool visibility) {}

// It returns if video window is already created and can be continued to next
// step.
bool WebMediaPlayerMSE::EnsureVideoWindowCreated() {
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

void WebMediaPlayerMSE::ContinuePlayerWithWindowId() {
  if (pending_request_.pending_load_) {
    WebMediaPlayerImpl::Load(pending_load_type_, pending_source_,
                             pending_cors_mode_, pending_is_cache_disabled_);
    pending_request_.pending_load_ = absl::nullopt;
  }
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerMSE::GetCurrentFrameFromCompositor() const {
  TRACE_EVENT0("media", "WebMediaPlayerMSE::GetCurrentFrameFromCompositor");

  return video_frame_provider_->GetCurrentFrame();
}

double WebMediaPlayerMSE::TimelineOffset() const {
  return WebMediaPlayerImpl::timelineOffset();
}

bool WebMediaPlayerMSE::UsesIntrinsicSize() const {
  return media::MediaPreferences::Get()->UseIntrinsicSizeForMSE();
}

void WebMediaPlayerMSE::SetRenderMode(WebMediaPlayer::RenderMode mode) {
  if (render_mode_ == mode)
    return;

  render_mode_ = mode;
#if defined(NEVA_VIDEO_HOLE)
  video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kHole);
#endif
}

void WebMediaPlayerMSE::SetDisableAudio(bool disable) {
  NEVA_LOGTF(INFO) << "disable=" << disable;
  media_platform_api_->SetDisableAudio(disable);
}

void WebMediaPlayerMSE::Suspend() {
  if (is_suspended_) {
    client_->DidMediaSuspended();
    return;
  }

  status_on_suspended_ = Paused() ? PausedStatus : PlayingStatus;

  if (status_on_suspended_ == PlayingStatus)
    client_->PausePlayback(WebMediaPlayerClient::PauseReason::kUnknown);

  if (media_platform_api_.get()) {
    media::SuspendReason reason =
        client_->IsSuppressedMediaPlay()
            ? media::SuspendReason::kBackgrounded
            : media::SuspendReason::kSuspendedByPolicy;
    media_platform_api_->Suspend(reason);
  }

  has_activation_permit_ = false;

  if (HasVideo())
    video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kBlack);

  // Usually we wait until OnSuspended(), but send OnMediaSuspended()
  // immediately when media_platform_api_ is null.
  if (!media_platform_api_.get())
    client_->DidMediaSuspended();
}

void WebMediaPlayerMSE::OnSuspended() {
  NEVA_LOGTF(INFO);
  is_suspended_ = true;
  client_->DidMediaSuspended();
}

void WebMediaPlayerMSE::OnMediaActivationPermitted() {
  // If we already have activation permit, just skip.
  if (has_activation_permit_) {
    client_->DidMediaActivated();
    return;
  }

  has_activation_permit_ = true;

  if (is_loading_) {
    OnLoadPermitted();
    return;
  } else if (is_suspended_) {
    Resume();
    return;
  }

  ProcessPendingRequests();

  client_->DidMediaActivated();
}

void WebMediaPlayerMSE::ProcessPendingRequests() {
  if (pending_request_.pending_rate_)
    SetRate(pending_request_.pending_rate_.value());

  if (pending_request_.pending_seek_time_)
    Seek(pending_request_.pending_seek_time_->InSecondsF());

  if (pending_request_.pending_play_) {
    Play();
    client_->ResumePlayback();
  }
}

void WebMediaPlayerMSE::OnMediaPlayerObserverConnectionEstablished() {
  client_->DidMediaCreated(require_media_resource_);
}

void WebMediaPlayerMSE::Resume() {
  if (!is_suspended_) {
    client_->DidMediaActivated();
    return;
  }

  is_suspended_ = false;

  media::RestorePlaybackMode restore_playback_mode;

  restore_playback_mode = (status_on_suspended_ == PausedStatus)
                              ? media::RestorePlaybackMode::kPaused
                              : media::RestorePlaybackMode::kPlaying;

  if (media_platform_api_.get())
    media_platform_api_->Resume(paused_time_, restore_playback_mode);
  else {
    // Usually we wait until OnResumed(), but send OnMediaActivated()
    // immediately when media_platform_api_ is null.
    client_->DidMediaActivated();
  }
}

void WebMediaPlayerMSE::OnResumed() {
  NEVA_LOGTF(INFO);

  // TODO(neva, sync-to-91):
  // Changed to calling base function due to missing interface in |client_|.
  // But need to ensure.
  WebMediaPlayerImpl::Seek(paused_time_.InSecondsF());

  if (status_on_suspended_ == PausedStatus)
    client_->PausePlayback(WebMediaPlayerClient::PauseReason::kUnknown);
  else
    client_->ResumePlayback();
  status_on_suspended_ = UnknownStatus;

  ProcessPendingRequests();

#if defined(NEVA_VIDEO_HOLE)
  if (HasVideo())
    video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kHole);
#endif

  client_->DidMediaActivated();
}

void WebMediaPlayerMSE::OnLoadPermitted() {
#if defined(USE_GAV)
  if (!EnsureVideoWindowCreated()) {
    pending_request_.pending_load_ = true;
    return;
  }
#endif
  WebMediaPlayerImpl::Load(pending_load_type_, pending_source_,
                           pending_cors_mode_, pending_is_cache_disabled_);
}

}  // namespace blink
