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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_NEVA_WEBRTC_VIDEO_DECODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_NEVA_WEBRTC_VIDEO_DECODER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace webrtc {
class SdpVideoFormat;
class VideoDecoder;
}  // namespace webrtc

namespace blink {

class PLATFORM_EXPORT NevaWebRtcVideoDecoderFactory
    : public webrtc::VideoDecoderFactory {
 public:
  explicit NevaWebRtcVideoDecoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);
  ~NevaWebRtcVideoDecoderFactory() override = default;

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;

 private:
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_NEVA_NEVA_WEBRTC_VIDEO_DECODER_FACTORY_H_
