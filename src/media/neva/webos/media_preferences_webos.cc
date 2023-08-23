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

#include "media/neva/media_preferences.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/jsoncpp/source/include/json/json.h"

namespace media {

MediaPreferences::MediaPreferences() {
#if defined(USE_GST_MEDIA)
  media_prefs_info_.is_av1_codec_enabled = false;
#endif
}

void MediaPreferences::Update(const std::string& media_preferences) {
  base::AutoLock auto_lock(lock_);

  if (raw_preferences == media_preferences)
    return;
  raw_preferences = media_preferences;

  // Format:
  // "mediaExtension":{"mse":{"enableAV1":true,
  //                          "disableVideoIntrinsicSize":true,
  //                          "maxAudioSourceBuffer":15,
  //                          "maxVideoSourceBuffer":100},
  //                   "ums":{"fixedAspectRatio":true }}
  Json::Value preferences;
  Json::Reader reader;

  if (!reader.parse(media_preferences, preferences))
    return;

  // MSE Preferences
  if (preferences.isMember("mse")) {
    if (preferences["mse"].isMember("disableVideoIntrinsicSize")) {
      media_prefs_info_.mse_use_intrinsic_size =
          !preferences["mse"]["disableVideoIntrinsicSize"].asBool();
    }
  }

  if (preferences.isMember("mse") && preferences["mse"].isMember("enableAV1")) {
    media_prefs_info_.is_av1_codec_enabled =
        preferences["mse"]["enableAV1"].asBool();
  }

  if (preferences.isMember("supportDolbyHDR")) {
    media_prefs_info_.is_supported_dolby_hdr =
        preferences["supportDolbyHDR"].asBool();
  }

  if (preferences.isMember("supportDolbyAtmos")) {
    media_prefs_info_.is_supported_dolby_atmos =
        preferences["supportDolbyAtmos"].asBool();
  }

  VLOG(1) << __func__ << " info: " << media_prefs_info_.ToString();
}

void MediaPreferences::SetMediaCodecCapabilities(
    const std::string& capabilities) {
  base::AutoLock auto_lock(lock_);

  Json::Value codec_capability;
  Json::Reader reader;

  if (!reader.parse(capabilities, codec_capability))
    return;

  Json::Value video_codecs = codec_capability["videoCodecs"];
  for (Json::Value::iterator iter = video_codecs.begin();
       iter != video_codecs.end(); iter++) {
    if (!(*iter).isObject())
      continue;

    std::string codec;

    // Need to match with upper ASCII of
    // media::GetCodecName(VideoCodec codec)
    const std::string name = base::ToUpperASCII((*iter)["name"].asString());
    if (name == "H.264")
      codec = "H264";
    else if (name == "MPEG2")
      codec = "MPEG2VIDEO";
    else {
      codec = name;
    }
    capabilities_.emplace_back(codec, (*iter)["maxWidth"].asInt(),
                               (*iter)["maxHeight"].asInt(),
                               (*iter)["maxFrameRate"].asInt(),
                               (*iter)["maxBitRate"].asInt() * 1024 * 1024, 0);
  }

  // Need to match with upper ASCII of
  // media::GetCodecName(AudioCodec codec)
  Json::Value audio_codecs = codec_capability["audioCodecs"];
  for (Json::Value::iterator iter = audio_codecs.begin();
       iter != audio_codecs.end(); iter++) {
    if (!(*iter).isObject())
      continue;

    const std::string name = base::ToUpperASCII((*iter)["name"].asString());
    int channels = (*iter)["channels"].asInt();
    if (name == "MPEG") {
      capabilities_.emplace_back("MP3", 0, 0, 0, 0, channels);
    } else if (name == "AMR") {
      capabilities_.emplace_back("AMR_WB", 0, 0, 0, 0, channels);
      capabilities_.emplace_back("AMR_NB", 0, 0, 0, 0, channels);
    } else {
      capabilities_.emplace_back(name, 0, 0, 0, 0, channels);
    }
  }
}

bool MediaPreferences::IsSupportedAudioType(const media::AudioType& type) {
  // Defer to media's default support.
  return media::IsDefaultSupportedAudioType(type);
}

bool MediaPreferences::IsSupportedVideoType(const media::VideoType& type) {
  // Defer to media's default support.
  return media::IsDefaultSupportedVideoType(type);
}

bool MediaPreferences::IsSupportedVideoCodec(
    const MediaCodecCapability& capability) {
  return true;
}

bool MediaPreferences::IsSupportedAudioCodec(
    const MediaCodecCapability& capability) {
  return true;
}

bool MediaPreferences::IsSupportedUHD() {
  return is_supported_uhd.has_value() ? is_supported_uhd.value() : false;
}

}  // namespace media
