// Copyright 2014-2020 LG Electronics, Inc.
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

#include "third_party/blink/renderer/platform/media/neva/web_media_player_neva.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "cc/layers/video_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_switches_neva.h"
#include "media/base/timestamp_constants.h"
#include "media/neva/media_constants.h"
#include "net/base/mime_util.h"
#include "neva/logging.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/platform/media/neva/media_info_loader.h"

using gpu::gles2::GLES2Interface;

using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebString;
using media::PipelineStatus;

namespace {

// Limits the range of playback rate.
const double kMinRate = -16.0;
const double kMaxRate = 16.0;

const char* ReadyStateToString(WebMediaPlayer::ReadyState state) {
#define STRINGIFY_READY_STATUS_CASE(state) \
  case WebMediaPlayer::ReadyState::state:  \
    return #state

  switch (state) {
    STRINGIFY_READY_STATUS_CASE(kReadyStateHaveNothing);
    STRINGIFY_READY_STATUS_CASE(kReadyStateHaveMetadata);
    STRINGIFY_READY_STATUS_CASE(kReadyStateHaveCurrentData);
    STRINGIFY_READY_STATUS_CASE(kReadyStateHaveFutureData);
    STRINGIFY_READY_STATUS_CASE(kReadyStateHaveEnoughData);
  }
  return "null";
}

const char* NetworkStateToString(WebMediaPlayer::NetworkState state) {
#define STRINGIFY_NETWORK_STATUS_CASE(state) \
  case WebMediaPlayer::NetworkState::state:  \
    return #state

  switch (state) {
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateEmpty);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateIdle);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateLoading);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateLoaded);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateFormatError);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateNetworkError);
    STRINGIFY_NETWORK_STATUS_CASE(kNetworkStateDecodeError);
  }
  return "null";
}

const char* MediaErrorToString(media::MediaPlayerNeva::MediaError error) {
#define STRINGIFY_MEDIA_ERROR_CASE(error)         \
  case media::MediaPlayerNeva::MediaError::error: \
    return #error

  switch (error) {
    STRINGIFY_MEDIA_ERROR_CASE(MEDIA_ERROR_NONE);
    STRINGIFY_MEDIA_ERROR_CASE(MEDIA_ERROR_FORMAT);
    STRINGIFY_MEDIA_ERROR_CASE(MEDIA_ERROR_DECODE);
    STRINGIFY_MEDIA_ERROR_CASE(MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK);
    STRINGIFY_MEDIA_ERROR_CASE(MEDIA_ERROR_INVALID_CODE);
  }
  return "null";
}

bool IsBackgroundedSuspendEnabled() {
#if 1
  /* TODO(neva): upstream changed that IsBackgroundSuspendEnabled() returns true
   * by default. As a result, it may cause any conflict with our suspend/resume
   * by FrameMediaController It needs to be revisited.
   */
  return false;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaSuspend);
#endif
}

class SyncTokenClientImpl : public media::VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncTokenClientImpl() override {}
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    gl_->GenSyncTokenCHROMIUM(sync_token->GetData());
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
};

}  // namespace

namespace blink {
WebMediaPlayer* WebMediaPlayerNeva::Create(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::MediaLog> media_log,
    WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
    scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    CreateVideoWindowCallback create_video_window_callback,
    const WebString& application_id,
    bool use_unlimited_media_policy,
    media::CreateMediaPlayerNevaCB create_media_player_neva_cb) {
  WebMediaPlayer::LoadType load_type = client->LoadType();
  media::MediaPlayerType media_player_type =
      media::MediaPlayerNevaFactory::GetMediaPlayerType(
          client->ContentMIMEType().Latin1());
  if (load_type == WebMediaPlayer::kLoadTypeURL &&
      media_player_type != media::MediaPlayerType::kMediaPlayerTypeNone)
    return new WebMediaPlayerNeva(
        frame, client, delegate, std::move(media_log), std::move(defer_load_cb),
        std::move(audio_renderer_sink), std::move(compositor_task_runner),
        media_player_type, std::move(create_video_window_callback),
        application_id, use_unlimited_media_policy,
        std::move(create_media_player_neva_cb));
  return nullptr;
}

bool WebMediaPlayerNeva::CanSupportMediaType(const std::string& mime) {
  if (media::MediaPlayerNevaFactory::GetMediaPlayerType(mime) ==
      media::MediaPlayerType::kMediaPlayerTypeNone)
    return false;
  else
    return true;
}

WebMediaPlayerNeva::WebMediaPlayerNeva(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::MediaLog> media_log,
    WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
    scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    media::MediaPlayerType media_player_type,
    CreateVideoWindowCallback create_video_window_callback,
    const WebString& application_id,
    bool use_unlimited_media_policy,
    media::CreateMediaPlayerNevaCB create_media_player_neva_cb)
    : frame_(frame),
      main_task_runner_(frame->GetTaskRunner(TaskType::kMediaElementEvent)),
      client_(client),
      delegate_(delegate),
      delegate_id_(0),
      defer_load_cb_(std::move(defer_load_cb)),
      seeking_(false),
      did_loading_progress_(false),
      create_media_player_neva_cb_(std::move(create_media_player_neva_cb)),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      is_playing_(false),
      has_size_info_(false),
      video_frame_provider_client_(NULL),
      media_log_(std::move(media_log)),
      interpolator_(&default_tick_clock_),
      playback_completed_(false),
      is_suspended_(client->IsSuppressedMediaPlay()),
      status_on_suspended_(UnknownStatus),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(compositor_task_runner
                                  ? std::move(compositor_task_runner)
                                  : base::ThreadTaskRunnerHandle::Get()),
      render_mode_(WebMediaPlayer::RenderModeNone),
      app_id_(application_id.Utf8().data()),
      is_loading_(false),
      create_video_window_callback_(std::move(create_video_window_callback)) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());

  weak_this_ = weak_factory_.GetWeakPtr();

  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);
  else {
    NEVA_LOGTF(ERROR)
        << "delegate_ is null. This may introduce unexpected behavior.";
  }

  media_log_->AddEvent<media::MediaLogEvent::kWebMediaPlayerCreated>(
      url::Origin(frame_->GetSecurityOrigin()).GetURL().spec());

  // Use the null sink if no sink was provided.
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      std::move(audio_renderer_sink), media_log_.get());

  if (!create_media_player_neva_cb_)
    create_media_player_neva_cb_ = base::BindRepeating(
        &media::MediaPlayerNevaFactory::CreateMediaPlayerNeva);

  player_api_.reset(create_media_player_neva_cb_.Run(
      this, media_player_type, main_task_runner_, app_id_));

  video_frame_provider_ =
      std::make_unique<VideoFrameProviderImpl>(compositor_task_runner_);
  video_frame_provider_->SetWebLocalFrame(frame);
  video_frame_provider_->SetWebMediaPlayerClient(client);
  SetRenderMode(GetClient()->RenderMode());
  absl::optional<bool> is_audio_disabled = GetClient()->IsAudioDisabled();
  if (is_audio_disabled.has_value())
    SetDisableAudio(*is_audio_disabled);

  require_media_resource_ =
      player_api_->RequireMediaResource() && !use_unlimited_media_policy;

  EnsureVideoWindowCreated();
}

WebMediaPlayerNeva::~WebMediaPlayerNeva() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  GetClient()->SetCcLayer(nullptr);

  if (video_layer_.get()) {
    video_layer_->StopUsingProvider();
  }
  compositor_task_runner_->DeleteSoon(FROM_HERE,
                                      std::move(video_frame_provider_));

  media_log_->OnWebMediaPlayerDestroyed();

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }
}

WebMediaPlayer::LoadTiming WebMediaPlayerNeva::Load(
    LoadType load_type,
    const WebMediaPlayerSource& src,
    CorsMode cors_mode,
    bool is_cache_disabled) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  NEVA_VLOGTF(1);

  NEVA_DCHECK(src.IsURL());

  // TODO(neva, sync-to-91): Check |is_cache_disabled| usage.

  is_loading_ = true;

  // If preloading is expected, do load without permit from MediaStateManager.
  if (player_api_->IsPreloadable(
          GetClient()->ContentMediaOption().Utf8().data())) {
    DoLoad(load_type, src.GetAsURL(), cors_mode);
    return LoadTiming::kImmediate;
  }

  pending_load_type_ = load_type;
  pending_source_ = WebMediaPlayerSource(src.GetAsURL());
  pending_cors_mode_ = cors_mode;

  client_->DidMediaActivationNeeded();

  return LoadTiming::kDeferred;
}

void WebMediaPlayerNeva::ProcessPendingRequests() {
  if (pending_request_.pending_rate_)
    SetRate(pending_request_.pending_rate_.value());

  if (pending_request_.pending_seek_time_)
    Seek(pending_request_.pending_seek_time_->InSecondsF());

  if (pending_request_.pending_play_) {
    Play();
    client_->ResumePlayback();
  }
}

void WebMediaPlayerNeva::UpdatePlayingState(bool is_playing) {
  NEVA_VLOGTF(1);
  if (is_playing == is_playing_)
    return;

  is_playing_ = is_playing;

  if (is_playing)
    interpolator_.StartInterpolating();
  else
    interpolator_.StopInterpolating();

  if (delegate_) {
    if (is_playing) {
      client_->DidPlayerStartPlaying();
      delegate_->DidPlay(delegate_id_);
    } else {
      // TODO(neva, sync-to-87): Even if OnPlaybackComplete() has not been
      // called yet, Blink may have already fired the ended event based on
      // current time relative to duration -- so we need to check both
      // possibilities here.
      client_->DidPlayerPaused(IsEnded());
      delegate_->DidPause(delegate_id_, IsEnded());
    }
  }
}

void WebMediaPlayerNeva::DoLoad(LoadType load_type,
                                const WebURL& url,
                                CorsMode cors_mode) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  // We should use MediaInfoLoader for all URLs but because of missing
  // scheme handlers in WAM we use it only for file scheme for now.
  // By using MediaInfoLoader url gets passed to network delegate which
  // does proper whitelist filtering for local file access.
  GURL mediaUrl(url);
  if (mediaUrl.SchemeIsFile() || mediaUrl.SchemeIsFileSystem()) {
    info_loader_.reset(new MediaInfoLoader(
        mediaUrl, base::BindRepeating(&WebMediaPlayerNeva::DidLoadMediaInfo,
                                      weak_this_)));
    info_loader_->Start(frame_);

    UpdateNetworkState(WebMediaPlayer::kNetworkStateLoading);
    UpdateReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  } else {
    UpdateNetworkState(WebMediaPlayer::kNetworkStateLoading);
    UpdateReadyState(WebMediaPlayer::kReadyStateHaveNothing);
    DidLoadMediaInfo(true, mediaUrl);
  }
}

void WebMediaPlayerNeva::DidLoadMediaInfo(bool ok, const GURL& url) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  if (!ok) {
    info_loader_.reset();
    UpdateNetworkState(WebMediaPlayer::kNetworkStateNetworkError);
    return;
  }

  media_log_->AddEvent<media::MediaLogEvent::kLoad>(url.spec());
  url_ = url;

  LoadMedia();
}

void WebMediaPlayerNeva::LoadMedia() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  NEVA_VLOGTF(1);

#if defined(USE_GAV)
  if (!EnsureVideoWindowCreated()) {
    pending_request_.pending_load_ = true;
    return;
  }
  pending_request_.pending_load_ = absl::nullopt;
#endif

  player_api_->Initialize(
      GetClient()->IsVideo(), CurrentTime(), url_.spec(),
      std::string(GetClient()->ContentMIMEType().Utf8().data()),
      std::string(GetClient()->Referrer().Utf8().data()),
      std::string(GetClient()->UserAgent().Utf8().data()),
      std::string(GetClient()->Cookies().Utf8().data()),
      std::string(GetClient()->ContentMediaOption().Utf8().data()),
      std::string(GetClient()->ContentCustomOption().Utf8().data()));
}

void WebMediaPlayerNeva::OnActiveRegionChanged(const gfx::Rect& active_region) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_LOGTF(INFO) << "(" << active_region.ToString() << ")";
  video_frame_provider_->ActiveRegionChanged(active_region);
  if (!NaturalSize().IsEmpty())
    video_frame_provider_->UpdateVideoFrame();
}

void WebMediaPlayerNeva::Play() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_LOGTF(INFO);
  if (!has_activation_permit_) {
    NEVA_LOGTF(INFO) << "block to play on suspended";
    status_on_suspended_ = PlayingStatus;
    pending_request_.pending_play_ = true;
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_play_ = absl::nullopt;

  UpdatePlayingState(true);
  player_api_->Start();
  // We think this time as if we have a first frame since platform mediaplayer
  // starts playing. If there is better point, it needs to go there.
  has_first_frame_ = true;

  media_log_->AddEvent<media::MediaLogEvent::kPlay>();
  client_->DidPlayerStartPlaying();
  if (delegate_)
    delegate_->DidPlay(delegate_id_);
}

void WebMediaPlayerNeva::Pause() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_LOGTF(INFO);

  UpdatePlayingState(false);
  player_api_->Pause();

  paused_time_ = base::Seconds(CurrentTime());

  media_log_->AddEvent<media::MediaLogEvent::kPause>();
  client_->DidPlayerPaused(IsEnded());

  if (delegate_) {
    delegate_->DidPause(delegate_id_, IsEnded());
  }
}

void WebMediaPlayerNeva::OnFrozen() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED_LOG_ONCE();
}

// TODO(wanchang): need to propagate to MediaPlayerNeva
bool WebMediaPlayerNeva::SupportsFullscreen() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  return true;
}

int WebMediaPlayerNeva::GetDelegateId() {
  return delegate_id_;
}

void WebMediaPlayerNeva::Seek(double seconds) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);

  base::TimeDelta new_seek_time = base::Seconds(seconds);

  if (!has_activation_permit_) {
    LOG(INFO) << "block to Seek on suspended";
    pending_request_.pending_seek_time_ = new_seek_time;
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_seek_time_ = absl::nullopt;

  playback_completed_ = false;

  if (seeking_) {
    if (new_seek_time == seek_time_) {
      // Suppress all redundant seeks if unrestricted by media source
      // demuxer API.
      return;
    }

    pending_request_.pending_seek_time_ = new_seek_time;

    return;
  }

  seeking_ = true;
  seek_time_ = new_seek_time;

  // Kick off the asynchronous seek!
  player_api_->Seek(seek_time_);
  media_log_->AddEvent<media::MediaLogEvent::kSeek>(seconds);
}

void WebMediaPlayerNeva::SetRate(double rate) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);

  // Limit rates to reasonable values by clamping.
  rate = std::max(kMinRate, std::min(rate, kMaxRate));

  if (!has_activation_permit_) {
    NEVA_LOGTF(INFO) << "block to SetRate on suspended";
    pending_request_.pending_rate_ = rate;
    if (!client_->IsSuppressedMediaPlay())
      client_->DidMediaActivationNeeded();
    return;
  }

  pending_request_.pending_rate_ = absl::nullopt;

  interpolator_.SetPlaybackRate(rate);
  player_api_->SetRate(rate);
  is_negative_playback_rate_ = rate < 0.0f;
  playback_rate_ = rate;
}

void WebMediaPlayerNeva::SetVolume(double volume) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  volume_ = volume;
  player_api_->SetVolume(volume_);

  // TODO(M108): Need to pass proper audio/video codec info
  client_->DidMediaMetadataChange(
      HasAudio(), HasVideo(), media::AudioCodec::kUnknown,
      media::VideoCodec::kUnknown,
      media::DurationToMediaContentType(base::Seconds(Duration())));

  if (delegate_) {
    delegate_->DidMediaMetadataChange(
        delegate_id_, HasAudio(), HasVideo(),
        media::DurationToMediaContentType(base::Seconds(Duration())));
  }
}

void WebMediaPlayerNeva::SetLatencyHint(double seconds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebMediaPlayerNeva::SetPreservesPitch(bool preserves_pitch) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebMediaPlayerNeva::SetWasPlayedWithUserActivation(
    bool was_played_with_user_activation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebMediaPlayerNeva::OnTimeUpdate() {
  media_session::MediaPosition new_position(
      Paused() ? 0.0 : playback_rate_, duration_, base::Seconds(CurrentTime()),
      IsEnded());

  if (media_position_state_ == new_position)
    return;

  media_position_state_ = new_position;

  if (client_)
    client_->DidPlayerMediaPositionStateChange(
        playback_rate_, duration_, base::Seconds(CurrentTime()), IsEnded());
}

void WebMediaPlayerNeva::SetPreload(Preload preload) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  if (!EnsureVideoWindowCreated()) {
    pending_request_.pending_preload_ = preload;
    return;
  }
  pending_request_.pending_preload_ = absl::nullopt;
  switch (preload) {
    case WebMediaPlayer::kPreloadNone:
      player_api_->SetPreload(media::MediaPlayerNeva::PreloadNone);
      break;
    case WebMediaPlayer::kPreloadMetaData:
      player_api_->SetPreload(media::MediaPlayerNeva::PreloadMetaData);
      break;
    case WebMediaPlayer::kPreloadAuto:
      player_api_->SetPreload(media::MediaPlayerNeva::PreloadAuto);
      break;
    default:
      break;
  }
}

bool WebMediaPlayerNeva::HasVideo() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(2);
  return player_api_->HasVideo();
}

bool WebMediaPlayerNeva::HasAudio() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(2);
  return player_api_->HasAudio();
}

bool WebMediaPlayerNeva::Paused() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  return !is_playing_;
}

bool WebMediaPlayerNeva::Seeking() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return seeking_;
}

double WebMediaPlayerNeva::Duration() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  return duration_.InSecondsF();
}

double WebMediaPlayerNeva::CurrentTime() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // If the player is processing a seek, return the seek time.
  // Blink may still query us if updatePlaybackState() occurs while seeking.
  if (Seeking()) {
    return pending_request_.pending_seek_time_
               ? pending_request_.pending_seek_time_->InSecondsF()
               : seek_time_.InSecondsF();
  }

  double current_time =
      std::min((const_cast<media::TimeDeltaInterpolator*>(&interpolator_))
                   ->GetInterpolatedTime(),
               duration_)
          .InSecondsF();

  // The time of interpolator updated from UMediaClient could be a little bigger
  // than the correct current time, this makes |current_time| a negative number
  // after the plaback time reaches at 0:00 by rewinding.
  // this conditional statement sets current_time's lower bound which is 00:00
  if (current_time < 0)
    current_time = 0;

  return current_time;
}

bool WebMediaPlayerNeva::IsEnded() const {
  return playback_completed_;
}

gfx::Size WebMediaPlayerNeva::NaturalSize() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return natural_size_;
}

gfx::Size WebMediaPlayerNeva::VisibleSize() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // FIXME: Need to check visible rect: really it is natural size.
  return natural_size_;
}

WebMediaPlayer::NetworkState WebMediaPlayerNeva::GetNetworkState() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerNeva::GetReadyState() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ready_state_;
}

WebString WebMediaPlayerNeva::GetErrorMessage() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return WebString();
}

bool WebMediaPlayerNeva::WouldTaintOrigin() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva) : Need to check return value
  return false;
}

WebTimeRanges WebMediaPlayerNeva::Buffered() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!player_api_)
    return WebTimeRanges();

  media::Ranges<base::TimeDelta> ranges = player_api_->GetBufferedTimeRanges();

  return ConvertToWebTimeRanges(ranges);
}

WebTimeRanges WebMediaPlayerNeva::Seekable() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return WebTimeRanges();

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration.  While not expected, disabling this breaks semi-live
  // players, http://crbug.com/427412.
  const WebTimeRange seekable_range(0.0, Duration());
  return WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerNeva::DidLoadingProgress() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

bool WebMediaPlayerNeva::SetSinkId(const WebString& sing_id,
                                   WebSetSinkIdCompleteCallback) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva, sync-to-91): Need to be investigated.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WebMediaPlayerNeva::SetVolumeMultiplier(double multiplier) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva, sync-to-91): Need to be investigated.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebMediaPlayerNeva::Paint(cc::PaintCanvas* canvas,
                               const gfx::Rect& rect,
                               cc::PaintFlags& flags) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(wanchang): check android impl
  return;
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerNeva::GetCurrentFrameThenUpdate() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva, sync-to-91): Need to be investigated.
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

absl::optional<int> WebMediaPlayerNeva::CurrentFrameId() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva): Need to be investigated
  NOTIMPLEMENTED_LOG_ONCE();
  return absl::nullopt;
}

double WebMediaPlayerNeva::MediaTimeForTimeValue(double timeValue) const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return base::Seconds(timeValue).InSecondsF();
}

unsigned WebMediaPlayerNeva::DecodedFrameCount() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(wanchang): check android impl
  return 0;
}

unsigned WebMediaPlayerNeva::DroppedFrameCount() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(wanchang): check android impl
  return 0;
}

uint64_t WebMediaPlayerNeva::AudioDecodedByteCount() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(wanchang): check android impl
  return 0;
}

uint64_t WebMediaPlayerNeva::VideoDecodedByteCount() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(wanchang): check android impl
  return 0;
}

bool WebMediaPlayerNeva::PassedTimingAllowOriginCheck() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void WebMediaPlayerNeva::SuspendForFrameClosed() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva, sync-to-91): Need to be investigated.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WebMediaPlayerNeva::HasAvailableVideoFrame() const {
  return has_first_frame_;
}

void WebMediaPlayerNeva::OnMediaMetadataChanged(base::TimeDelta duration,
                                                const gfx::Size& coded_size,
                                                const gfx::Size& natural_size,
                                                bool success) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1);
  bool need_to_signal_duration_changed = false;

  // For HLS streams, the reported duration may be zero for infinite streams.
  // See http://crbug.com/501213.
  if (duration.is_zero() && IsHLSStream())
    duration = media::kInfiniteDuration;

  // Update duration, if necessary, prior to ready state updates that may
  // cause duration() query.
  if (duration_ != duration) {
    duration_ = duration;
    // Client readyState transition from HAVE_NOTHING to HAVE_METADATA
    // already triggers a durationchanged event. If this is a different
    // transition, remember to signal durationchanged.
    if (ready_state_ > WebMediaPlayer::kReadyStateHaveNothing) {
      need_to_signal_duration_changed = true;
    }
  }

  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata) {
    UpdateReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  }

  // TODO(wolenetz): Should we just abort early and set network state to an
  // error if success == false? See http://crbug.com/248399
  if (success) {
    OnVideoSizeChanged(coded_size, natural_size);
  }

  if (need_to_signal_duration_changed)
    client_->DurationChanged();

  // TODO(M108): Need to pass proper audio/video codec info
  client_->DidMediaMetadataChange(
      HasAudio(), HasVideo(), media::AudioCodec::kUnknown,
      media::VideoCodec::kUnknown,
      media::DurationToMediaContentType(base::Seconds(Duration())));

  if (delegate_) {
    delegate_->DidMediaMetadataChange(
        delegate_id_, HasAudio(), HasVideo(),
        media::DurationToMediaContentType(base::Seconds(Duration())));
  }
}

void WebMediaPlayerNeva::OnLoadComplete() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  is_loading_ = false;
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveEnoughData)
    UpdateReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  client_->DidMediaActivated();
}

void WebMediaPlayerNeva::OnPlaybackComplete() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // When playback is about to finish, android media player often stops
  // at a time which is smaller than the duration. This makes webkit never
  // know that the playback has finished. To solve this, we set the
  // current time to media duration when OnPlaybackComplete() get called.
  // But in case of negative playback, we set the current time to zero.
  base::TimeDelta bound =
      is_negative_playback_rate_ ? base::TimeDelta() : duration_;
  interpolator_.SetBounds(
      bound, bound,
      base::TimeTicks::Now());  // TODO(wanchang): fix 3rd argument
  playback_completed_ = true;
  client_->TimeChanged();

  // If the loop attribute is set, timeChanged() will update the current time
  // to 0. It will perform a seek to 0. Issue a command to the player to start
  // playing after seek completes.
  if (is_playing_ && seeking_ && seek_time_.is_zero())
    player_api_->Start();
}

void WebMediaPlayerNeva::OnBufferingStateChanged(
    const media::BufferingState buffering_state) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  did_loading_progress_ = true;

  switch (buffering_state) {
    case media::BufferingState::BUFFERING_HAVE_NOTHING:
      interpolator_.StopInterpolating();
      UpdateReadyState(WebMediaPlayer::kReadyStateHaveCurrentData);
      break;
    case media::BufferingState::BUFFERING_HAVE_ENOUGH:
      if (is_playing_)
        interpolator_.StartInterpolating();
      UpdateReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
      if (network_state_ < WebMediaPlayer::kNetworkStateLoaded)
        UpdateNetworkState(WebMediaPlayer::kNetworkStateLoaded);
      break;
    default:
      NOTREACHED() << "Invalid buffering state";
  }
}

void WebMediaPlayerNeva::OnSeekComplete(const base::TimeDelta& current_time) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  seeking_ = false;
  if (pending_request_.pending_seek_time_) {
    Seek(pending_request_.pending_seek_time_->InSecondsF());
    return;
  }
  interpolator_.SetBounds(current_time, current_time, base::TimeTicks::Now());

  UpdateReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);

  client_->TimeChanged();
}

void WebMediaPlayerNeva::OnMediaError(int error_type) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_LOGTF(ERROR) << "("
                    << MediaErrorToString(
                           (media::MediaPlayerNeva::MediaError)error_type)
                    << ")";

  if (is_loading_) {
    is_loading_ = false;
    client_->DidMediaActivated();
  }

  switch (error_type) {
    case media::MediaPlayerNeva::MEDIA_ERROR_FORMAT:
      UpdateNetworkState(WebMediaPlayer::kNetworkStateFormatError);
      break;
    case media::MediaPlayerNeva::MEDIA_ERROR_DECODE:
      UpdateNetworkState(WebMediaPlayer::kNetworkStateDecodeError);
      break;
    case media::MediaPlayerNeva::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
      UpdateNetworkState(WebMediaPlayer::kNetworkStateFormatError);
      break;
    case media::MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE:
      break;
  }
  client_->Repaint();
}

void WebMediaPlayerNeva::OnVideoSizeChanged(const gfx::Size& coded_size,
                                            const gfx::Size& natural_size) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1) << "coded_size: " << coded_size.ToString()
                 << " / natural_size: " << natural_size.ToString();

  // Ignore OnVideoSizeChanged before kReadyStateHaveMetadata.
  // OnVideoSizeChanged will be called again from OnMediaMetadataChanged
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return;

  // For HLS streams, a bogus empty size may be reported at first, followed by
  // the actual size only once playback begins. See http://crbug.com/509972.
  if (!has_size_info_ && natural_size.width() == 0 &&
      natural_size.height() == 0 && IsHLSStream()) {
    return;
  }

  has_size_info_ = true;
  if (natural_size_ == natural_size)
    return;

  coded_size_ = coded_size;
  natural_size_ = natural_size;

  client_->SizeChanged();

  if (video_window_remote_)
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
  // set video size first then update videoframe since videoframe
  // needs video size.
  video_frame_provider_->SetNaturalVideoSize(NaturalSize());
  video_frame_provider_->UpdateVideoFrame();

  // Lazily allocate compositing layer.
  if (!video_layer_) {
    video_layer_ = cc::VideoLayer::Create(video_frame_provider_.get(),
                                          media::VIDEO_ROTATION_0);
    client_->SetCcLayer(video_layer_.get());

    // If we're paused after we receive metadata for the first time, tell the
    // delegate we can now be safely suspended due to inactivity if a subsequent
    // play event does not occur.
    if (Paused() && delegate_) {
      client_->DidPlayerPaused(IsEnded());
      delegate_->DidPause(delegate_id_, IsEnded());
    }
  }
}

void WebMediaPlayerNeva::OnAudioTracksUpdated(
    const std::vector<media::MediaTrackInfo>& audio_track_info) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  for (auto& audio_track : audio_track_info) {
    // Check current id is already added or not.
    auto it = std::find_if(audio_track_ids_.begin(), audio_track_ids_.end(),
                           [&audio_track](const MediaTrackId& id) {
                             return audio_track.id == id.second;
                           });
    if (it != audio_track_ids_.end())
      continue;

    // TODO(neva): Use kind info. And as per comment in WebMediaPlayerImpl,
    // only the first audio track is enabled by default to match blink logic.
    WebMediaPlayer::TrackId track_id = GetClient()->AddAudioTrack(
        WebString::FromUTF8(audio_track.id),
        WebMediaPlayerClient::kAudioTrackKindMain,
        WebString::FromUTF8("Audio Track"),
        WebString::FromUTF8(audio_track.language), false);
    if (!track_id.IsNull() && !track_id.IsEmpty())
      audio_track_ids_.push_back(MediaTrackId(track_id, audio_track.id));
  }

  // TODO(neva): Should we remove unavailable audio track?
}

void WebMediaPlayerNeva::OnTimeUpdate(base::TimeDelta current_timestamp,
                                      base::TimeTicks current_time_ticks) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (Seeking())
    return;

  // Compensate the current_timestamp with the IPC latency.
  base::TimeDelta lower_bound =
      base::TimeTicks::Now() - current_time_ticks + current_timestamp;
  base::TimeDelta upper_bound = lower_bound;
  // We should get another time update in about |kTimeUpdateInterval|
  // milliseconds.
  if (is_playing_) {
    upper_bound += base::Milliseconds(media::kTimeUpdateInterval);
  }

  if (lower_bound > upper_bound)
    upper_bound = lower_bound;
  interpolator_.SetBounds(
      lower_bound, upper_bound,
      current_time_ticks);  // TODO(wanchang): check 3rd argument
}

void WebMediaPlayerNeva::OnMediaPlayerPlay() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  UpdatePlayingState(true);
  client_->ResumePlayback();
}

void WebMediaPlayerNeva::OnMediaPlayerPause() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  UpdatePlayingState(false);
  client_->PausePlayback(WebMediaPlayerClient::PauseReason::kUnknown);
}

void WebMediaPlayerNeva::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  NEVA_VLOGTF(1) << "(" << NetworkStateToString(state) << ")";
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing &&
      (state == WebMediaPlayer::kNetworkStateNetworkError ||
       state == WebMediaPlayer::kNetworkStateDecodeError)) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    network_state_ = WebMediaPlayer::kNetworkStateFormatError;
  } else {
    network_state_ = state;
  }
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkStateChanged();
}

void WebMediaPlayerNeva::UpdateReadyState(WebMediaPlayer::ReadyState state) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  NEVA_VLOGTF(1) << "(" << ReadyStateToString(state) << ")";

  if (state == WebMediaPlayer::kReadyStateHaveEnoughData &&
      url_.SchemeIs("file") &&
      network_state_ == WebMediaPlayer::kNetworkStateLoading)
    UpdateNetworkState(WebMediaPlayer::kNetworkStateLoaded);

  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->ReadyStateChanged();
}

base::WeakPtr<WebMediaPlayer> WebMediaPlayerNeva::AsWeakPtr() {
  return weak_this_;
}

scoped_refptr<WebAudioSourceProviderImpl>
WebMediaPlayerNeva::GetAudioSourceProvider() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return audio_source_provider_;
}

void WebMediaPlayerNeva::Repaint() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(GetClient());
  GetClient()->Repaint();
}

WebMediaPlayerClient* WebMediaPlayerNeva::GetClient() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_DCHECK(client_);
  return client_;
}

bool WebMediaPlayerNeva::UsesIntrinsicSize() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return player_api_->UsesIntrinsicSize();
}

WebString WebMediaPlayerNeva::MediaId() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  return WebString::FromUTF8(player_api_->MediaId());
}

bool WebMediaPlayerNeva::HasAudioFocus() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva): Actually this API is deprecated. Clean up.
  return true;
}

void WebMediaPlayerNeva::SetAudioFocus(bool focus) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  // TODO(neva): Actually this API is deprecated. Clean up.
}

void WebMediaPlayerNeva::OnCustomMessage(
    const media::MediaEventType media_event_type,
    const std::string& detail) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  NEVA_VLOGTF(1) << "detail: " << detail;

  WebMediaPlayer::MediaEventType converted_event_type =
      WebMediaPlayer::kMediaEventNone;

  switch (media_event_type) {
    case media::MediaEventType::kMediaEventUpdateUMSMediaInfo:
      converted_event_type =
          WebMediaPlayer::MediaEventType::kMediaEventUpdateUMSMediaInfo;
      break;
#if defined(USE_NEVA_MEDIA_PLAYER_CAMERA)
    case media::MediaEventType::kMediaEventUpdateCameraState:
      converted_event_type =
          WebMediaPlayer::MediaEventType::kMediaEventUpdateCameraState;
      break;
#endif  // defined(USE_NEVA_MEDIA_PLAYER_CAMERA)
    default:
      converted_event_type = WebMediaPlayer::MediaEventType::kMediaEventNone;
      break;
  }

  client_->SendCustomMessage(converted_event_type, WebString::FromUTF8(detail));
}

void WebMediaPlayerNeva::SetRenderMode(WebMediaPlayer::RenderMode mode) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (render_mode_ == mode)
    return;

  render_mode_ = mode;
#if defined(NEVA_VIDEO_HOLE)
  video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kHole);
#endif
}

void WebMediaPlayerNeva::SetDisableAudio(bool disable) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (audio_disabled_ == disable)
    return;
  NEVA_LOGTF(INFO) << "disable=" << disable;
  audio_disabled_ = disable;
  player_api_->SetDisableAudio(disable);
}

void WebMediaPlayerNeva::Suspend() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (is_suspended_) {
    client_->DidMediaSuspended();
    return;
  }

  is_suspended_ = true;
  has_activation_permit_ = false;
  status_on_suspended_ = Paused() ? PausedStatus : PlayingStatus;
  if (status_on_suspended_ == PlayingStatus) {
    client_->PausePlayback(WebMediaPlayerClient::PauseReason::kUnknown);
  }
  if (HasVideo()) {
    video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kBlack);
  }
  media::SuspendReason reason = client_->IsSuppressedMediaPlay()
                                    ? media::SuspendReason::kBackgrounded
                                    : media::SuspendReason::kSuspendedByPolicy;
  player_api_->Suspend(reason);
  client_->DidMediaSuspended();
}

void WebMediaPlayerNeva::OnMediaActivationPermitted() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
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

void WebMediaPlayerNeva::OnMediaPlayerObserverConnectionEstablished() {
  client_->DidMediaCreated(require_media_resource_);
}

void WebMediaPlayerNeva::Resume() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!is_suspended_) {
    client_->DidMediaActivated();
    return;
  }

  is_suspended_ = false;

  if (HasVideo()) {
#if defined(NEVA_VIDEO_HOLE)
    video_frame_provider_->SetFrameType(VideoFrameProviderImpl::kHole);
#endif
    video_frame_provider_->UpdateVideoFrame();
  }

  if (!player_api_->IsRecoverableOnResume()) {
    player_api_.reset(create_media_player_neva_cb_.Run(
        this,
        media::MediaPlayerNevaFactory::GetMediaPlayerType(
            client_->ContentMIMEType().Latin1()),
        main_task_runner_, app_id_));
    if (video_window_info_)
      player_api_->SetMediaLayerId(video_window_info_->native_window_id);
    player_api_->SetVolume(volume_);
    LoadMedia();
  } else {
    player_api_->Resume();
  }

  if (status_on_suspended_ == PlayingStatus) {
    client_->ResumePlayback();
    client_->DidPlayerStartPlaying();
    status_on_suspended_ = UnknownStatus;
  }

  ProcessPendingRequests();

  client_->DidMediaActivated();
}

void WebMediaPlayerNeva::OnLoadPermitted() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());

  NEVA_VLOGTF(1);
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::BindOnce(
        &WebMediaPlayerNeva::DoLoad, weak_this_, pending_load_type_,
        pending_source_.GetAsURL(), pending_cors_mode_));
    return;
  }

  DoLoad(pending_load_type_, pending_source_.GetAsURL(), pending_cors_mode_);
}

void WebMediaPlayerNeva::EnabledAudioTracksChanged(
    const WebVector<TrackId>& enabled_track_ids) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto it = std::find_if(
      audio_track_ids_.begin(), audio_track_ids_.end(),
      [&enabled_track_ids](const MediaTrackId& id) {
        return enabled_track_ids[enabled_track_ids.size() - 1] == id.first;
      });
  if (it != audio_track_ids_.end())
    player_api_->SelectTrack(media::MediaTrackType::kAudio, it->second);
}

bool WebMediaPlayerNeva::IsHLSStream() const {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  const GURL& url = redirected_url_.is_empty() ? url_ : redirected_url_;
  return (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile()) &&
         url.spec().find("m3u8") != std::string::npos;
}

void WebMediaPlayerNeva::OnMediaSourceOpened(WebMediaSource* web_media_source) {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->MediaSourceOpened(web_media_source);
}

void WebMediaPlayerNeva::OnVideoWindowCreated(const ui::VideoWindowInfo& info) {
  video_window_info_ = info;
  video_frame_provider_->SetOverlayPlaneId(info.window_id);
  player_api_->SetMediaLayerId(info.native_window_id);
  if (!coded_size_.IsEmpty() || !natural_size_.IsEmpty())
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
  ContinuePlayerWithWindowId();
}

void WebMediaPlayerNeva::OnVideoWindowDestroyed() {
  video_window_info_ = absl::nullopt;
  video_window_client_receiver_.reset();
}

void WebMediaPlayerNeva::OnVideoWindowGeometryChanged(const gfx::Rect& rect) {}

void WebMediaPlayerNeva::OnVideoWindowVisibilityChanged(bool visibility) {}

// It returns if video window is already created and can be continued to next
// step.
bool WebMediaPlayerNeva::EnsureVideoWindowCreated() {
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

void WebMediaPlayerNeva::ContinuePlayerWithWindowId() {
  if (pending_request_.pending_preload_)
    SetPreload(pending_request_.pending_preload_.value());
  if (pending_request_.pending_load_)
    LoadMedia();
}

void WebMediaPlayerNeva::OnFrameHidden() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!IsBackgroundedSuspendEnabled())
    return;

  Suspend();
}

void WebMediaPlayerNeva::OnFrameShown() {
  NEVA_DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!IsBackgroundedSuspendEnabled())
    return;

  Resume();
}

bool WebMediaPlayerNeva::Send(const std::string& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  VLOG(1) << "message:  " << message;
  if (message.empty())
    return false;

  return player_api_->Send(message);
}

}  // namespace blink
