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

#include "third_party/blink/renderer/platform/peerconnection/neva/neva_webrtc_video_decoder_factory.h"

#include "third_party/blink/renderer/platform/peerconnection/neva/webrtc_pass_through_video_decoder.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/media/engine/internal_decoder_factory.h"

namespace blink {

NevaWebRtcVideoDecoderFactory::NevaWebRtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : gpu_factories_(gpu_factories),
      media_task_runner_(std::move(media_task_runner)) {
  // We cannot hold empty supported format list because DecoderAdapter
  // requires precise supported format.
  // So now we borrow supported formats from SW codec factory.
  webrtc::InternalDecoderFactory software_decoder_factory;
  supported_formats_ = software_decoder_factory.GetSupportedFormats();
}

std::vector<webrtc::SdpVideoFormat>
NevaWebRtcVideoDecoderFactory::GetSupportedFormats() const {
  return supported_formats_;
}

std::unique_ptr<webrtc::VideoDecoder>
NevaWebRtcVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  return std::move(WebRtcPassThroughVideoDecoder::Create(
      gpu_factories_, media_task_runner_, format));
}

}  // namespace blink
