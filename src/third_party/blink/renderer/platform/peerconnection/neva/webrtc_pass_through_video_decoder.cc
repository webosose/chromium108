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

#include "third_party/blink/renderer/platform/peerconnection/neva/webrtc_pass_through_video_decoder.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_frame.h"
#include "media/neva/media_preferences.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/api/video/encoded_image.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"
#include "third_party/webrtc/rtc_base/helpers.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

const char* kImplementationName = "WebRtcPassThroughVideoDecoder";

// Any reasonable size, will be overridden by the decoder anyway.
constexpr gfx::Size kDefaultSize(640, 480);

// Maximum number of frames that we will queue in |pending_buffers_|.
constexpr int32_t kMaxPendingBuffers = 8;

// Maximum number of timestamps that will be maintained in |decode_timestamps_|.
// Really only needs to be a bit larger than the maximum reorder distance (which
// is presumably 0 for WebRTC), but being larger doesn't hurt much.
constexpr int32_t kMaxDecodeHistory = 32;

// Maximum number of consecutive frames that can fail to decode before
// requesting fallback to software decode.
constexpr int32_t kMaxConsecutiveErrors = 5;

// Maximum number seconds to wait for initialization to complete
constexpr int32_t kTimeoutSeconds = 10;

// Maximum number requests for decoder for smooth operation
constexpr int32_t kMaxDecodeRequests = 4;

// Number of Decoder instances right now that have started decoding.
class DecoderCounter {
 public:
  int Count() { return count_.load(); }

  void IncrementCount() {
    int c = ++count_;
    DCHECK_GT(c, 0);
  }

  void DecrementCount() {
    int c = --count_;
    DCHECK_GE(c, 0);
  }

 private:
  std::atomic_int count_{0};
};

DecoderCounter* GetDecoderCounter() {
  static DecoderCounter s_counter;
  // Note that this will init only in the first call in the ctor, so it's still
  // single threaded.
  return &s_counter;
}

void FinishWait(base::WaitableEvent* waiter, bool* result_out, bool result) {
  *result_out = result;
  waiter->Signal();
}

struct EncodedImageExternalMemory
    : public media::DecoderBuffer::ExternalMemory {
 public:
  explicit EncodedImageExternalMemory(
      rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> buffer_interface)
      : ExternalMemory(base::make_span(buffer_interface->data(),
                                       buffer_interface->size())),
        buffer_interface_(std::move(buffer_interface)) {}
  ~EncodedImageExternalMemory() override = default;

 private:
  rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> buffer_interface_;
};

}  // namespace

#define BIND_TO_MAIN_TASK(function)     \
  base::BindPostTask(main_task_runner_, \
                     base::BindRepeating(function, weak_this_), FROM_HERE)

// static
std::unique_ptr<WebRtcPassThroughVideoDecoder>
WebRtcPassThroughVideoDecoder::Create(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const webrtc::SdpVideoFormat& sdp_format) {
  VLOG(1) << __func__ << "(" << sdp_format.name << ")";

  // Bail early for unknown codecs.
  media::VideoCodec video_codec = WebRtcToMediaVideoCodec(
      webrtc::PayloadStringToCodecType(sdp_format.name));
  if (video_codec == media::VideoCodec::kUnknown)
    return nullptr;

  // Fallback to software decoder if not supported by platform.
  const std::string& codec =
      base::ToUpperASCII(media::GetCodecName(video_codec));
  const auto capability =
      media::MediaPreferences::Get()->GetMediaCodecCapabilityForCodec(codec);
  if (!capability.has_value()) {
    VLOG(1) << __func__ << " " << codec << " is unsupported by HW";
    return nullptr;
  }

  media::VideoDecoderConfig video_config(
      video_codec, WebRtcVideoFormatToMediaVideoCodecProfile(sdp_format),
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation, kDefaultSize, gfx::Rect(kDefaultSize),
      kDefaultSize, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);
  video_config.set_live_stream(true);
  video_config.set_hdr_metadata(gfx::HDRMetadata());

  // Synchronously verify that the decoder can be initialized.
  std::unique_ptr<WebRtcPassThroughVideoDecoder> video_decoder =
      base::WrapUnique(new WebRtcPassThroughVideoDecoder(
          gpu_factories, media_task_runner, video_codec));
  if (video_decoder->InitializeSync(video_config)) {
    return video_decoder;
  }

  // Initialization failed - post delete task and try next supported
  // implementation, if any.
  gpu_factories->GetTaskRunner()->DeleteSoon(FROM_HERE,
                                             std::move(video_decoder));

  return nullptr;
}

WebRtcPassThroughVideoDecoder::WebRtcPassThroughVideoDecoder(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    media::VideoCodec video_codec)
    : video_codec_(video_codec),
      app_id_(gpu_factories->GetAppId()),
      main_task_runner_(gpu_factories->GetTaskRunner()),
      media_task_runner_(std::move(media_task_runner)),
      create_video_window_callback_(gpu_factories->GetCreateVideoWindowCB()) {
  VLOG(1) << __func__ << "[" << this << "] "
          << " app_id_=" << app_id_
          << " codec: " << media::GetCodecName(video_codec);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

WebRtcPassThroughVideoDecoder::~WebRtcPassThroughVideoDecoder() {
  VLOG(1) << __func__ << "[" << this << "] ";

  base::AutoLock auto_lock(lock_);

  if (have_started_decoding_)
    GetDecoderCounter()->DecrementCount();

  if (media_platform_api_)
    media_platform_api_->Finalize();
}

bool WebRtcPassThroughVideoDecoder::Configure(
    const webrtc::VideoDecoder::Settings& settings) {
  VLOG(1) << __func__ << "[" << this << "] "
          << " codec: " << GetCodecName(video_codec_)
          << " has_error_=" << has_error_;

  base::AutoLock auto_lock(lock_);

  // Save the initial resolution so that we can fall back later, if needed.
  current_resolution_ =
      static_cast<int32_t>(settings.max_render_resolution().Width()) *
      settings.max_render_resolution().Height();

  return !has_error_;
}

int32_t WebRtcPassThroughVideoDecoder::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    int64_t render_time_ms) {
  DVLOG(2) << __func__ << "[" << this << "] "
           << " render_time_ms=" << render_time_ms;

  // If this is the first decode, then increment the count of working decoders.
  if (!have_started_decoding_) {
    have_started_decoding_ = true;
    GetDecoderCounter()->IncrementCount();
  }

  // Don't allow hardware decode for small videos if there are too many
  // decoder instances. This includes the case where our resolution drops while
  // too many decoders exist.
  {
    base::AutoLock auto_lock(lock_);
    if (current_resolution_ < kMinResolution &&
        GetDecoderCounter()->Count() > kMaxDecoderInstances) {
      // Decrement the count and clear the flag, so that other decoders don't
      // fall back also.
      have_started_decoding_ = false;
      GetDecoderCounter()->DecrementCount();
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }

  // Hardware VP9 decoders don't handle more than one spatial layer. Fall back
  // to software decoding. See https://crbug.com/webrtc/9304.
  if (video_codec_ == media::VideoCodec::kVP9 &&
      input_image.SpatialIndex().value_or(0) > 0) {
    LOG(WARNING) << __func__
                 << " VP9 with more spatial index > 0. Fallback to s/w Decoder";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  if (missing_frames) {
    VLOG(1) << __func__ << " Missing or incomplete frames";
    // We probably can't handle broken frames. Request a key frame.
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!input_image.data() || !input_image.size()) {
    LOG(ERROR) << __func__ << " Invalid Encoded Image";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  bool is_key_frame =
      input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey;
  {
    base::AutoLock auto_lock(lock_);
    if (key_frame_required_) {
      // We discarded previous frame because we have too many pending frames
      // (see logic) below. Now we need to wait for the key frame and discard
      // everything else.
      if (!is_key_frame) {
        VLOG(2) << __func__ << " Discard non-key frame";
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      VLOG(2) << __func__ << " Key frame received, resume decoding";
      // ok, we got key frame and can continue decoding
      key_frame_required_ = false;
    }

    if (is_key_frame) {
      frame_size_.set_width(input_image._encodedWidth);
      frame_size_.set_height(input_image._encodedHeight);
    }
  }

  const base::TimeDelta frame_timestamp =
      base::Microseconds(input_image.Timestamp());

  DCHECK(input_image.GetEncodedData());
  auto decode_buffer = media::DecoderBuffer::FromExternalMemory(
      std::make_unique<EncodedImageExternalMemory>(
          input_image.GetEncodedData()));
  DCHECK(decode_buffer);

  decode_buffer->set_is_key_frame(is_key_frame);
  decode_buffer->set_timestamp(frame_timestamp);

  // Queue for decoding.
  size_t pending_buffers_size = pending_buffers_.size();
  {
    base::AutoLock auto_lock(lock_);

    if (has_error_) {
      LOG(WARNING) << __func__ << " Got error. Fallback to software";
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }

    if (pending_buffers_.size() >= kMaxPendingBuffers) {
      // We are severely behind. Drop pending frames and request a keyframe to
      // catch up as quickly as possible.
      VLOG(1) << __func__ << " Pending frames overflow. Request keyframe";
      pending_buffers_.clear();

      // Actually we just discarded a frame. We must wait for the key frame and
      // drop any other non-key frame.
      key_frame_required_ = true;
      if (++consecutive_error_count_ > kMaxConsecutiveErrors) {
        decode_timestamps_.clear();
        LOG(WARNING) << __func__ << " error_count=" << consecutive_error_count_
                     << ", Fallback to software";
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      }
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    pending_buffers_.push_back(std::move(decode_buffer));
  }

  if (pending_buffers_size == 0) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcPassThroughVideoDecoder::DecodeOnMediaThread,
                       weak_this_));
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebRtcPassThroughVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  VLOG(1) << __func__ << "[" << this << "] has_error: " << has_error_;

  base::AutoLock auto_lock(lock_);
  decode_complete_callback_ = callback;

  return has_error_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                    : WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebRtcPassThroughVideoDecoder::Release() {
  VLOG(1) << __func__ << "[" << this << "] has_error: " << has_error_;

  base::AutoLock auto_lock(lock_);
  pending_buffers_.clear();
  decode_timestamps_.clear();

  DestroyMediaPlatformAPI();

  return has_error_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                    : WEBRTC_VIDEO_CODEC_OK;
}

webrtc::VideoDecoder::DecoderInfo
WebRtcPassThroughVideoDecoder::GetDecoderInfo() const {
  DecoderInfo info;
  info.implementation_name = kImplementationName;
  info.is_hardware_accelerated = true;
  return info;
}

void WebRtcPassThroughVideoDecoder::OnVideoWindowCreated(
    const ui::VideoWindowInfo& info) {
  VLOG(1) << "[" << this << "] " << __func__
          << " window_id=" << info.native_window_id
          << " overlay_plane_id=" << info.window_id;

  video_window_info_ = info;

  if (video_window_created_cb_)
    std::move(video_window_created_cb_).Run(video_window_info_.has_value());
}

void WebRtcPassThroughVideoDecoder::OnVideoWindowDestroyed() {
  VLOG(1) << "[" << this << "] " << __func__;
  video_window_info_ = absl::nullopt;
  video_window_client_receiver_.reset();
}

void WebRtcPassThroughVideoDecoder::OnVideoWindowGeometryChanged(
    const gfx::Rect& rect) {
  VLOG(1) << "[" << this << "] " << __func__ << " rect=" << rect.ToString();
}

void WebRtcPassThroughVideoDecoder::OnVideoWindowVisibilityChanged(
    bool visibility) {
  VLOG(1) << "[" << this << "] " << __func__ << " visibility=" << visibility;
}

bool WebRtcPassThroughVideoDecoder::InitializeSync(
    const media::VideoDecoderConfig& video_config) {
  DCHECK(!media_task_runner_->RunsTasksInCurrentSequence());

  VLOG(1) << __func__ << "[" << this << "]";

  bool result = EnsureVideoWindowCreated();
  if (result) {
    video_config_ = video_config;
    result = EnsureMediaPlatformApiCreated();
  }

  VLOG(1) << __func__ << " result: " << (result ? "Success" : "Fail");
  return result;
}

void WebRtcPassThroughVideoDecoder::DecodeOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  DVLOG(2) << __func__ << " pending_buffers_size=" << pending_buffers_.size();
  while (outstanding_decode_requests_ < kMaxDecodeRequests) {
    scoped_refptr<media::DecoderBuffer> buffer;
    {
      base::AutoLock auto_lock(lock_);

      if (has_error_ || pending_buffers_.empty())
        return;

      // Take the first pending buffer.
      buffer = pending_buffers_.front();
      pending_buffers_.pop_front();

      // Record the timestamp.
      while (decode_timestamps_.size() >= kMaxDecodeHistory)
        decode_timestamps_.pop_front();
      decode_timestamps_.push_back(buffer->timestamp());
    }

    // Submit for decoding.
    outstanding_decode_requests_++;
    const base::TimeDelta buffer_timestamp = buffer->timestamp();
    if (media_platform_api_->Feed(std::move(buffer), media::FeedType::kVideo)) {
      DVLOG(2) << __func__ << " Feed Success! ts=" << buffer_timestamp;
      media_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WebRtcPassThroughVideoDecoder::ReturnOutputFrame,
                         weak_this_, buffer_timestamp));
    } else {
      VLOG(1) << __func__ << "[" << this << "]: Entering permanent error state";
      decode_timestamps_.clear();
      pending_buffers_.clear();
      has_error_ = true;
      return;
    }
  }
}

void WebRtcPassThroughVideoDecoder::ReturnOutputFrame(
    const base::TimeDelta& timestamp) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  DVLOG(2) << __func__ << "[" << this << "]";

  outstanding_decode_requests_--;

  // Make a shallow copy.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateVideoHoleFrame(video_window_info_->window_id,
                                              frame_size_, timestamp);
  if (!video_frame) {
    LOG(ERROR) << __func__ << " Could not allocate video_frame.";
    return;
  }

  webrtc::VideoFrame rtc_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(
              rtc::scoped_refptr<blink::WebRtcVideoFrameAdapter>(
                  new rtc::RefCountedObject<blink::WebRtcVideoFrameAdapter>(
                      std::move(video_frame))))
          .set_timestamp_rtp(static_cast<uint32_t>(timestamp.InMicroseconds()))
          .set_timestamp_us(0)
          .set_rotation(webrtc::kVideoRotation_0)
          .build();

  base::AutoLock auto_lock(lock_);

  // Update `current_resolution_`, in case it's changed.  This lets us fall back
  // to software, or avoid doing so, if we're over the decoder limit.
  current_resolution_ =
      static_cast<int32_t>(rtc_frame.width()) * rtc_frame.height();

  if (!base::Contains(decode_timestamps_, timestamp)) {
    LOG(WARNING) << __func__ << "[" << this << "]"
                 << " Discarding frame with timestamp: " << timestamp;
    return;
  }

  decode_timestamps_.pop_front();

  DCHECK(decode_complete_callback_);
  decode_complete_callback_->Decoded(rtc_frame);
  consecutive_error_count_ = 0;
}

void WebRtcPassThroughVideoDecoder::CreateMediaPlatformAPI() {
  VLOG(1) << __func__;

  if (media_platform_api_)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> mpa_task_runner =
      static_cast<base::SingleThreadTaskRunner*>(media_task_runner_.get());

  // Create MediaPlatformAPI
  media_platform_api_ = media::MediaPlatformAPI::Create(
      mpa_task_runner, true, app_id_,
      BIND_TO_MAIN_TASK(&WebRtcPassThroughVideoDecoder::OnVideoSizeChanged),
      base::RepeatingClosure(), base::RepeatingClosure(),
      media::MediaPlatformAPI::ActiveRegionCB(),
      BIND_TO_MAIN_TASK(&WebRtcPassThroughVideoDecoder::OnPipelineError));

  if (!media_platform_api_) {
    LOG(ERROR) << __func__ << " Could not create media_platform_api";
    if (pipeline_init_cb_)
      std::move(pipeline_init_cb_).Run(false);
    return;
  }

  media_platform_api_->SetMediaPreferences(
      media::MediaPreferences::Get()->GetRawMediaPreferences());
  media_platform_api_->SetMediaCodecCapabilities(
      media::MediaPreferences::Get()->GetMediaCodecCapabilities());

  media_platform_api_->SetMediaLayerId(video_window_info_->native_window_id);
  media_platform_api_->SetDisableAudio(true);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcPassThroughVideoDecoder::InitMediaPlatformAPI,
                     weak_this_));
}

void WebRtcPassThroughVideoDecoder::DestroyMediaPlatformAPI() {
  VLOG(1) << __func__;

  if (!media_platform_api_)
    return;

  media_platform_api_->Finalize();
  media_platform_api_ = nullptr;
}

void WebRtcPassThroughVideoDecoder::InitMediaPlatformAPI() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  VLOG(1) << __func__ << " frame_size: " << frame_size_.ToString();

  media::AudioDecoderConfig audio_config;  // Just video data only
  media_platform_api_->Initialize(
      audio_config, video_config_,
      base::BindRepeating(
          &WebRtcPassThroughVideoDecoder::OnMediaPlatformAPIInitialized,
          weak_this_));
}

void WebRtcPassThroughVideoDecoder::OnMediaPlatformAPIInitialized(
    media::PipelineStatus status) {
  VLOG(1) << __func__ << " status : " << status;

  if (!media_platform_api_) {
    LOG(ERROR) << __func__ << " media platform api not available";
    return;
  }

  {
    has_error_ = (status != media::PIPELINE_OK);
    pipeline_running_ = (status == media::PIPELINE_OK);
  }

  if (pipeline_running_)
    media_platform_api_->SetPlaybackRate(1.0f);

  if (pipeline_init_cb_)
    std::move(pipeline_init_cb_).Run(pipeline_running_);
}

void WebRtcPassThroughVideoDecoder::OnPipelineError(
    media::PipelineStatus status) {
  LOG(ERROR) << __func__ << "[" << this << "] "
             << " status : " << status;

  {
    base::AutoLock auto_lock(lock_);
    pending_buffers_.clear();
    decode_timestamps_.clear();
    has_error_ = (status != media::PIPELINE_OK);
    pipeline_running_ = false;
  }

  if (pipeline_init_cb_)
    std::move(pipeline_init_cb_).Run(false);
}

void WebRtcPassThroughVideoDecoder::OnVideoSizeChanged(
    const gfx::Size& coded_size,
    const gfx::Size& natural_size) {
  VLOG(1) << "[" << this << "] " << __func__
          << " coded_size: " << coded_size.ToString()
          << " natural_size: " << natural_size.ToString();

  if (natural_size_ == natural_size)
    return;

  coded_size_ = coded_size;
  natural_size_ = natural_size;

  if (video_window_remote_)
    video_window_remote_->SetVideoSize(coded_size_, natural_size_);
}

bool WebRtcPassThroughVideoDecoder::EnsureVideoWindowCreated() {
  bool result = false;
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto video_window_cb = base::BindOnce(&FinishWait, base::Unretained(&waiter),
                                        base::Unretained(&result));

  video_window_created_cb_ = std::move(video_window_cb);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcPassThroughVideoDecoder::CreateVideoWindow,
                     weak_this_));

  // We will wait for the video window creation to complete
  if (!waiter.TimedWait(base::Seconds(kTimeoutSeconds))) {
    LOG(WARNING) << __func__ << " Video window creation timed out.";
    return false;
  }

  VLOG(1) << __func__ << " : " << (result ? "Success" : "Fail");
  return result;
}

bool WebRtcPassThroughVideoDecoder::EnsureMediaPlatformApiCreated() {
  bool result = false;
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto pipeline_init_cb = base::BindOnce(&FinishWait, base::Unretained(&waiter),
                                         base::Unretained(&result));
  pipeline_init_cb_ = std::move(pipeline_init_cb);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcPassThroughVideoDecoder::CreateMediaPlatformAPI,
                     weak_this_));

  // We will wait for the media pipeline creation & initialization to complete
  if (!waiter.TimedWait(base::Seconds(kTimeoutSeconds))) {
    LOG(WARNING) << __func__ << " Medid pipeline creation timed out.";
    return false;
  }

  VLOG(1) << __func__ << " : " << (result ? "Success" : "Fail");
  return result;
}

void WebRtcPassThroughVideoDecoder::CreateVideoWindow() {
  VLOG(1) << "[" << this << "] " << __func__;

  if (video_window_info_) {
    if (video_window_created_cb_)
      std::move(video_window_created_cb_).Run(true);
    return;
  }

  // |is_bound()| would be true if we already requested so we need to just wait
  // for response
  if (video_window_client_receiver_.is_bound())
    return;

  mojo::PendingRemote<ui::mojom::VideoWindowClient> pending_client;
  video_window_client_receiver_.Bind(
      pending_client.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<ui::mojom::VideoWindow> pending_window_remote;
  create_video_window_callback_.Run(
      std::move(pending_client),
      pending_window_remote.InitWithNewPipeAndPassReceiver(),
      ui::VideoWindowParams());
  video_window_remote_.Bind(std::move(pending_window_remote));
}

}  // namespace blink
