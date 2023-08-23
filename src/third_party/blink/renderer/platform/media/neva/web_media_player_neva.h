// Copyright 2014 LG Electronics, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/pipeline.h"
#include "media/base/time_delta_interpolator.h"
#include "media/base/video_frame.h"
#include "media/neva/media_player_neva_factory.h"
#include "media/neva/media_player_neva_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/platform/media/neva/create_video_window_callback.h"
#include "third_party/blink/public/platform/media/neva/video_frame_provider_impl.h"
#include "third_party/blink/public/platform/media/web_media_player_builder.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "ui/platform_window/neva/mojom/video_window.mojom.h"
#include "url/gurl.h"

namespace cc_blink {
class WebLayerImpl;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class VideoLayer;
}

namespace gpu {
struct SyncToken;
}

namespace content {
class RenderThreadImpl;
}  // namespace content

namespace media {
class MediaLog;
}

namespace blink {

class MediaInfoLoader;
class WebAudioSourceProviderImpl;
class WebLocalFrame;

class BLINK_PLATFORM_EXPORT WebMediaPlayerNeva
    : public WebMediaPlayer,
      public WebMediaPlayerDelegate::Observer,
      public ui::mojom::VideoWindowClient,
      public media::MediaPlayerNevaClient {
 public:
  enum StatusOnSuspended {
    UnknownStatus = 0,
    PlayingStatus,
    PausedStatus,
  };

  using MediaTrackId = std::pair<WebMediaPlayer::TrackId, std::string>;

  static bool CanSupportMediaType(const std::string& mime);
  static WebMediaPlayer* Create(
      WebLocalFrame* frame,
      WebMediaPlayerClient* client,
      WebMediaPlayerDelegate* delegate,
      std::unique_ptr<media::MediaLog> media_log,
      WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
      scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      CreateVideoWindowCallback create_video_window_cb,
      const WebString& application_id,
      const WebString& file_security_origin,
      bool use_unlimited_media_policy,
      media::CreateMediaPlayerNevaCB create_media_player_neva_cb);

  // This class implements WebMediaPlayer by keeping the private media
  // player api which is supported by target platform
  WebMediaPlayerNeva(
      WebLocalFrame* frame,
      WebMediaPlayerClient* client,
      WebMediaPlayerDelegate* delegate,
      std::unique_ptr<media::MediaLog> media_log,
      WebMediaPlayerBuilder::DeferLoadCB defer_load_cb,
      scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      media::MediaPlayerType media_player_type,
      CreateVideoWindowCallback create_video_window_cb,
      const WebString& application_id,
      const WebString& file_security_origin,
      bool use_unlimited_media_policy,
      media::CreateMediaPlayerNevaCB create_media_player_neva_cb);
  WebMediaPlayerNeva(const WebMediaPlayerNeva&) = delete;
  WebMediaPlayerNeva& operator=(const WebMediaPlayerNeva&) = delete;
  ~WebMediaPlayerNeva() override;

  WebMediaPlayer::LoadTiming Load(LoadType load_type,
                                  const WebMediaPlayerSource& src,
                                  CorsMode cors_mode,
                                  bool is_cache_disabled) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;
  void SetLatencyHint(double seconds) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void SetWasPlayedWithUserActivation(
      bool was_played_with_user_activation) override;
  void OnRequestPictureInPicture() override {}
  void OnTimeUpdate() override;

  WebTimeRanges Buffered() const override;
  WebTimeRanges Seekable() const override;

  void OnFrozen() override;

  // Attempts to switch the audio output device.
  // Implementations of SetSinkId take ownership of the WebSetSinkCallbacks
  // object.
  // Note also that SetSinkId implementations must make sure that all
  // methods of the WebSetSinkCallbacks object, including constructors and
  // destructors, run in the same thread where the object is created
  // (i.e., the blink thread).
  bool SetSinkId(const WebString& sing_id,
                 WebSetSinkIdCompleteCallback) override;

  void SetVolumeMultiplier(double multiplier) override;

  // Methods for painting.
  // FIXME: This path "only works" on Android. It is a workaround for the
  // issue that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES texture
  // internally. It should be removed and replaced by the normal paint path.
  // https://code.google.com/p/skia/issues/detail?id=1189
  void Paint(cc::PaintCanvas* canvas,
             const gfx::Rect& rect,
             cc::PaintFlags& flags) override;

  scoped_refptr<media::VideoFrame> GetCurrentFrameThenUpdate() override;

  absl::optional<int> CurrentFrameId() const override;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  bool SupportsFullscreen() const;
  int GetDelegateId() override;
  void SetPreload(Preload) override;

  // Dimensions of the video.
  gfx::Size NaturalSize() const override;

  gfx::Size VisibleSize() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  double CurrentTime() const override;
  bool IsEnded() const override;

  // Internal states of loading and network.
  WebMediaPlayer::NetworkState GetNetworkState() const override;
  WebMediaPlayer::ReadyState GetReadyState() const override;

  WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override;
  bool WouldTaintOrigin() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  uint64_t AudioDecodedByteCount() const override;
  uint64_t VideoDecodedByteCount() const override;

  bool PassedTimingAllowOriginCheck() const override;

  void SuspendForFrameClosed() override;

  // Returns true if the player has a frame available for presentation. Usually
  // this just means the first frame has been delivered.
  bool HasAvailableVideoFrame() const override;

  // media::MediaPlayerNevaClinet callback implementation
  void OnMediaMetadataChanged(base::TimeDelta duration,
                              const gfx::Size& coded_size,
                              const gfx::Size& natural_size,
                              bool success) override;
  void OnLoadComplete() override;
  void OnPlaybackComplete() override;
  void OnBufferingStateChanged(
      const media::BufferingState buffering_state) override;
  // void OnBufferingUpdate(double begin, double end) override;
  void OnSeekComplete(const base::TimeDelta& current_time) override;
  void OnMediaError(int error_type) override;
  void OnVideoSizeChanged(const gfx::Size& coded_size,
                          const gfx::Size& natural_size) override;
  void OnAudioTracksUpdated(
      const std::vector<media::MediaTrackInfo>& audio_track_info) override;

  // Called to update the current time.
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override;

  // Functions called when media player status changes.
  void OnMediaPlayerPlay()
      override;  // TODO(wanchang): need to check if it is required
  void OnMediaPlayerPause()
      override;  // TODO(wanchang): need to check if it is required

  void OnCustomMessage(const media::MediaEventType,
                       const std::string& detail) override;

  base::WeakPtr<WebMediaPlayer> AsWeakPtr() override;

  // WebMediaPlayerDelegate::Observer interface stubs
  void OnFrameHidden() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override {}

  scoped_refptr<WebAudioSourceProviderImpl> GetAudioSourceProvider() override;

  void OnActiveRegionChanged(const gfx::Rect&) override;

  void Repaint();

  void SetOpaque(bool opaque);

  bool UseVideoTexture();

  // neva::WebMediaPlayer implementaions
  bool UsesIntrinsicSize() const override;
  WebString MediaId() const override;
  bool HasAudioFocus() const override;
  void SetAudioFocus(bool focus) override;
  void SetRenderMode(WebMediaPlayer::RenderMode mode) override;
  void SetDisableAudio(bool) override;
  void Suspend() override;
  void OnMediaActivationPermitted() override;
  void OnMediaPlayerObserverConnectionEstablished() override;

  void Resume();
  void OnLoadPermitted();

  void EnabledAudioTracksChanged(
      const WebVector<TrackId>& enabledTrackIds) override;
  bool Send(const std::string& message) override;

 private:
  void ProcessPendingRequests();

  // void OnPipelinePlaybackStateChanged(bool playing);
  void UpdatePlayingState(bool is_playing);

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void UpdateNetworkState(WebMediaPlayer::NetworkState state);
  void UpdateReadyState(WebMediaPlayer::ReadyState state);

  bool IsHLSStream() const;

  void DoLoad(LoadType load_type, const WebURL& url, CorsMode cors_mode);
  void DidLoadMediaInfo(bool ok, const GURL& redirected_url);
  void LoadMedia();

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(const GURL& gurl, bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();

  // Notifies blink of the video size change.
  // void OnVideoSizeChange();

  // for MSE implementation
  void OnMediaSourceOpened(WebMediaSource* web_media_source);

  // Called when a decoder detects that the key needed to decrypt the stream
  // is not available.
  // void OnWaitingForDecryptionKey() override;
  // end of MSE implementation
  //-----------------------------------------------------

  // Implements ui::mojom::VideoWindowClient
  void OnVideoWindowCreated(const ui::VideoWindowInfo& info) override;
  void OnVideoWindowDestroyed() override;
  void OnVideoWindowGeometryChanged(const gfx::Rect& rect) override;
  void OnVideoWindowVisibilityChanged(bool visibility) override;
  // End of ui::mojom::VideoWindowClient

  bool EnsureVideoWindowCreated();
  void ContinuePlayerWithWindowId();

  WebLocalFrame* frame_;

  // Task runner for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  WebMediaPlayerClient* client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  WebMediaPlayerDelegate* delegate_;
  int delegate_id_;

  // Callback responsible for determining if loading of media should be deferred
  // for external reasons; called during load().
  WebMediaPlayerBuilder::DeferLoadCB defer_load_cb_;

  // Size of the video.
  gfx::Size coded_size_;
  gfx::Size natural_size_;

  // The video frame object used for rendering by the compositor.
  scoped_refptr<media::VideoFrame> current_frame_;
  base::Lock current_frame_lock_;

  // URL of the media file to be fetched.
  GURL url_;

  // URL of the media file after |media_info_loader_| resolves all the
  // redirections.
  GURL redirected_url_;

  // Media duration.
  base::TimeDelta duration_;

  double volume_ = 0.0f;

  bool is_negative_playback_rate_ = false;

  // Internal seek state.
  bool seeking_;
  base::TimeDelta seek_time_;

  // Whether loading has progressed since the last call to didLoadingProgress.
  bool did_loading_progress_;

  // Private MediaPlayer API instance
  std::unique_ptr<media::MediaPlayerNeva> player_api_;
  media::CreateMediaPlayerNevaCB create_media_player_neva_cb_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  WebMediaPlayer::NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;

  // Whether the media player is playing.
  bool is_playing_;

  // Whether the video size info is available.
  bool has_size_info_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  // Accessed on main thread and on compositor thread when main thread is
  // blocked.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_refptr<cc::VideoLayer> video_layer_;

  std::unique_ptr<media::MediaLog> media_log_;

  std::unique_ptr<MediaInfoLoader> info_loader_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  media::TimeDeltaInterpolator interpolator_;

  // TODO(wanchang): fix it
  // std::unique_ptr<MediaSourceDelegate> media_source_delegate_;

  // Whether OnPlaybackComplete() has been called since the last playback.
  bool playback_completed_;

  base::TimeDelta paused_time_;

  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_;

  bool is_suspended_;
  StatusOnSuspended status_on_suspended_;

  base::RepeatingTimer paintTimer_;

  std::vector<MediaTrackId> audio_track_ids_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<VideoFrameProviderImpl> video_frame_provider_;
  WebMediaPlayer::RenderMode render_mode_;

  std::string app_id_;
  std::string file_security_origin_;

  bool is_loading_;
  LoadType pending_load_type_ = WebMediaPlayer::kLoadTypeURL;
  WebMediaPlayerSource pending_source_;
  CorsMode pending_cors_mode_ = WebMediaPlayer::kCorsModeUnspecified;

  bool has_activation_permit_ = false;
  bool require_media_resource_ = true;

  bool audio_disabled_ = false;

  bool has_first_frame_ = false;
  CreateVideoWindowCallback create_video_window_callback_;
  absl::optional<ui::VideoWindowInfo> video_window_info_ = absl::nullopt;

  PendingRequest pending_request_;

  mojo::Remote<ui::mojom::VideoWindow> video_window_remote_;
  mojo::Receiver<ui::mojom::VideoWindowClient> video_window_client_receiver_{
      this};

  media_session::MediaPosition media_position_state_;
  double playback_rate_ = 1.0f;

  base::WeakPtr<WebMediaPlayerNeva> weak_this_;
  base::WeakPtrFactory<WebMediaPlayerNeva> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_WEB_MEDIA_PLAYER_NEVA_H_
