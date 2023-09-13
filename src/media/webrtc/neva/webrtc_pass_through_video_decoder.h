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

#ifndef MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
#define MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/neva/media_platform_api.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "ui/gfx/geometry/size.h"

namespace webrtc {
class EncodedImage;
class VideoCodec;
struct SdpVideoFormat;
}  // namespace webrtc

namespace media {

class VideoFrame;

class MEDIA_EXPORT WebRtcPassThroughVideoDecoder : public webrtc::VideoDecoder {
 public:
  // Minimum resolution that we'll consider "not low resolution" for the purpose
  // of falling back to software.
  static constexpr int32_t kMinResolution = 320 * 240;

  // Maximum number of decoder instances we'll allow before fallback to software
  // if the resolution is too low.  We'll allow more than this for high
  // resolution streams, but they'll fall back if they adapt below the limit.
  static constexpr int32_t kMaxDecoderInstances = 8;

  // Creates and initializes an WebRtcPassThroughVideoDecoder.
  static std::unique_ptr<WebRtcPassThroughVideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const webrtc::SdpVideoFormat& format);

  WebRtcPassThroughVideoDecoder(const WebRtcPassThroughVideoDecoder&) = delete;
  WebRtcPassThroughVideoDecoder& operator=(
      const WebRtcPassThroughVideoDecoder&) = delete;

  virtual ~WebRtcPassThroughVideoDecoder();

  // Implements webrtc::VideoDecoder
  bool Configure(const Settings& settings) override;
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  int32_t Release() override;
  DecoderInfo GetDecoderInfo() const override;

  // Callback to get pipeline data from WebMediaPlayerWebRTC
  void OnMediaPlayerInitCb(const std::string& app_id,
                           const std::string& window_id,
                           const base::RepeatingClosure& suspend_done_cb,
                           const base::RepeatingClosure& resume_done_cb,
                           const MediaPlatformAPI::VideoSizeChangedCB& size_cb,
                           const MediaPlatformAPI::ActiveRegionCB& region_cb);

  // Callback to get the status from WebMediaPlayerWebRTC
  void OnMediaPlayerSuspendCb(bool suspend);

 private:
  // Called on the worker thread.
  WebRtcPassThroughVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      media::VideoCodec video_codec);

  void DecodeOnMediaThread();
  void ReturnEmptyOutputFrame(const base::TimeDelta& timestamp);
  void SendEmptyRtcFrame(scoped_refptr<media::VideoFrame> encoded_frame);

  void CreateMediaPlatformAPI();
  void DestroyMediaPlatformAPI();

  void InitMediaPlatformAPI();
  void ReleaseMediaPlatformAPI();

  void OnMediaPlatformAPIInitialized(media::PipelineStatus status);
  void OnPipelineError(media::PipelineStatus status);

  bool InitializeMediaPlayer(const base::TimeDelta& start_time);

  // Construction parameters.
  media::VideoCodec video_codec_;

  gfx::Size frame_size_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_ = nullptr;

  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;

  std::string app_id_;
  std::string window_id_;

  bool is_render_mode_texture_ = false;

  bool is_destroying_ = false;
  bool is_suspended_ = false;

  bool player_load_notified_ = false;

  int32_t outstanding_decode_requests_ = 0;
  bool key_frame_required_ = true;

  bool have_started_decoding_ = false;

  // Shared members.
  base::Lock lock_;
  webrtc::VideoCodecType video_codec_type_ = webrtc::kVideoCodecGeneric;
  int32_t consecutive_error_count_ = 0;
  bool has_error_ = false;

  base::OnceCallback<void(bool)> pipeline_init_cb_;

  base::RepeatingCallback<void(const std::string&,
                               const std::string&,
                               const base::RepeatingClosure&,
                               const base::RepeatingClosure&,
                               const MediaPlatformAPI::VideoSizeChangedCB&,
                               const MediaPlatformAPI::ActiveRegionCB&)>
      media_player_init_cb_;
  base::RepeatingCallback<void(bool)> media_player_suspend_cb_;

  scoped_refptr<media::MediaPlatformAPI> media_platform_api_ = nullptr;

  // Requests that have not been submitted to the decoder yet.
  std::deque<scoped_refptr<media::DecoderBuffer>> pending_buffers_;

  // Record of timestamps that have been sent to be decoded. Removing a
  // timestamp will cause the frame to be dropped when it is output.
  std::deque<base::TimeDelta> decode_timestamps_;

  int32_t current_resolution_ = 0;

  base::RepeatingClosure suspend_done_cb_;
  base::RepeatingClosure resume_done_cb_;
  MediaPlatformAPI::VideoSizeChangedCB video_size_changed_cb_;
  MediaPlatformAPI::ActiveRegionCB active_region_cb_;

  base::WeakPtr<WebRtcPassThroughVideoDecoder> weak_this_;
  base::WeakPtrFactory<WebRtcPassThroughVideoDecoder> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_WEBRTC_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
