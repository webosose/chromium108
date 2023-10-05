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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/neva/media_platform_api.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/media/neva/create_video_window_callback.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/neva/mojom/video_window.mojom.h"

namespace webrtc {
class EncodedImage;
class VideoCodec;
struct SdpVideoFormat;
}  // namespace webrtc

namespace media {
class GpuVideoAcceleratorFactories;
class VideoFrame;
}  // namespace media

namespace blink {

class PLATFORM_EXPORT WebRtcPassThroughVideoDecoder
    : public webrtc::VideoDecoder,
      public ui::mojom::VideoWindowClient {
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
      media::GpuVideoAcceleratorFactories* gpu_factories,
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

  // Implements ui::mojom::VideoWindowClient
  void OnVideoWindowCreated(const ui::VideoWindowInfo& info) override;
  void OnVideoWindowDestroyed() override;
  void OnVideoWindowGeometryChanged(const gfx::Rect& rect) override;
  void OnVideoWindowVisibilityChanged(bool visibility) override;
  // End of mojom::VideoWindowClient

 private:
  // Called on the worker thread.
  WebRtcPassThroughVideoDecoder(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      media::VideoCodec video_codec);

  bool InitializeSync(const media::VideoDecoderConfig& video_config);

  void DecodeOnMediaThread();
  void ReturnOutputFrame(const base::TimeDelta& timestamp);

  void CreateMediaPlatformAPI();
  void DestroyMediaPlatformAPI();

  void InitMediaPlatformAPI();

  void OnMediaPlatformAPIInitialized(media::PipelineStatus status);
  void OnPipelineError(media::PipelineStatus status);
  void OnVideoSizeChanged(const gfx::Size& coded_size,
                          const gfx::Size& natural_size);

  bool EnsureVideoWindowCreated();
  bool EnsureMediaPlatformApiCreated();

  void CreateVideoWindow();

  media::VideoCodec video_codec_;
  media::VideoDecoderConfig video_config_;

  std::string app_id_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_ = nullptr;

  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;

  bool key_frame_required_ = true;
  bool have_started_decoding_ = false;
  bool pipeline_running_ = false;

  int32_t outstanding_decode_requests_ = 0;

  // Shared members.
  base::Lock lock_;
  int32_t consecutive_error_count_ = 0;
  bool has_error_ = false;

  scoped_refptr<media::MediaPlatformAPI> media_platform_api_ = nullptr;

  // Requests that have not been submitted to the decoder yet.
  std::deque<scoped_refptr<media::DecoderBuffer>> pending_buffers_;

  // Record of timestamps that have been sent to be decoded. Removing a
  // timestamp will cause the frame to be dropped when it is output.
  std::deque<base::TimeDelta> decode_timestamps_;

  int32_t current_resolution_ = 0;

  gfx::Size frame_size_;
  gfx::Size coded_size_;
  gfx::Size natural_size_;

  base::OnceCallback<void(bool)> pipeline_init_cb_;
  base::OnceCallback<void(bool)> video_window_created_cb_;

  CreateVideoWindowCallback create_video_window_callback_;
  absl::optional<ui::VideoWindowInfo> video_window_info_ = absl::nullopt;
  mojo::Remote<ui::mojom::VideoWindow> video_window_remote_;
  mojo::Receiver<ui::mojom::VideoWindowClient> video_window_client_receiver_{
      this};

  base::WeakPtr<WebRtcPassThroughVideoDecoder> weak_this_;
  base::WeakPtrFactory<WebRtcPassThroughVideoDecoder> weak_this_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_WEBRTC_PASS_THROUGH_VIDEO_DECODER_H_
