// Copyright 2018 LG Electronics, Inc.
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

#include "media/neva/media_util.h"

#include "base/strings/string_number_conversions.h"

namespace media {

// Transform Pixel aspect ratio string to width/height
VideoAspectRatio ParsePixelAspectRatio(const std::string& par) {
  size_t pos = par.find(":");
  if (pos == std::string::npos)
    return VideoAspectRatio();

  std::string w;
  std::string h;
  w.assign(par, 0, pos);
  h.assign(par, pos + 1, par.size() - pos - 1);

  unsigned int width;
  unsigned int height;
  if (base::StringToUint(w, &width) && base::StringToUint(h, &height) &&
      width > 0 && height > 0) {
    return VideoAspectRatio::PAR(width, height);
  }
  return VideoAspectRatio();
}

struct Media3DInfo GetMedia3DInfo(const std::string& media_3dinfo) {
  struct Media3DInfo res;
  res.type = "LR";

  if (media_3dinfo.find("RL") != std::string::npos) {
    res.pattern.assign(media_3dinfo, 0, media_3dinfo.size() - 3);
    res.type = "RL";
  } else if (media_3dinfo.find("LR") != std::string::npos) {
    res.pattern.assign(media_3dinfo, 0, media_3dinfo.size() - 3);
    res.type = "LR";
  } else if (media_3dinfo == "bottom_top") {
    res.pattern = "top_bottom";
    res.type = "RL";
  } else {
    res.pattern = media_3dinfo;
  }

  return res;
}

MediaPlayerNeva::MediaError ConvertToMediaError(PipelineStatus status) {
  switch (status.code()) {
    case PIPELINE_OK:
      return MediaPlayerNeva::MEDIA_ERROR_NONE;

    case PIPELINE_ERROR_NETWORK:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;

    case PIPELINE_ERROR_DECODE:
      return MediaPlayerNeva::MEDIA_ERROR_DECODE;

    case PIPELINE_ERROR_DECRYPT:
    case PIPELINE_ERROR_ABORT:
    case PIPELINE_ERROR_INITIALIZATION_FAILED:
    case PIPELINE_ERROR_COULD_NOT_RENDER:
    case PIPELINE_ERROR_READ:
    case PIPELINE_ERROR_INVALID_STATE:
      return MediaPlayerNeva::MEDIA_ERROR_FORMAT;

    // Demuxer related errors.
    case DEMUXER_ERROR_COULD_NOT_OPEN:
    case DEMUXER_ERROR_COULD_NOT_PARSE:
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;

    // Decoder related errors.
    case DECODER_ERROR_NOT_SUPPORTED:
      return MediaPlayerNeva::MEDIA_ERROR_FORMAT;

    // Resource is released by policy action
    case PIPELINE_ERROR_RESOURCE_IS_RELEASED:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;

    // ChunkDemuxer related errors.
    case CHUNK_DEMUXER_ERROR_APPEND_FAILED:
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;

    // Audio rendering errors.
    case AUDIO_RENDERER_ERROR:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;

    default:
      return MediaPlayerNeva::MEDIA_ERROR_INVALID_CODE;
  }
}

}  // namespace media
