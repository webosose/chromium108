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

#include <sstream>

#include "base/strings/pattern.h"

namespace {

static const std::pair<std::string, std::string> kCodecList[] = {
    // H.264
    {"avc1.*", "H264"},
    {"avc3.*", "H264"},
    // DOLBY VISION :disabled by enable_platform_dolby_vision
    // It is supported in chromecast only now.
    {"dva1.*", "DOLBYVISION"},
    {"dvav.*", "DOLBYVISION"},
    // HEVC
    {"dvh1.*", "HEVC"},
    {"dvhe.*", "HEVC"},
    {"hev1.*", "HEVC"},
    {"hvc1.*", "HEVC"},
    // VP8
    {"vp8", "VP8"},
    // VP9
    {"vp9", "VP9"},
    {"vp09.*", "VP9"},
    // AV1
    {"av01.*", "AV1"},

    // AUDIO Codecs
    {"vorbis", "VORBIS"},
    {"opus", "OPUS"},
    {"mp4a.40.*", "AAC"},
    {"mp4a.67", "AAC"},
    {"mp4a.a5", "AC3"},
    {"mp4a.A5", "AC3"},
    {"mp4a.a6", "EAC3"},
    {"mp4a.A6", "EAC3"},
    {"mp4a.69", "MPEG"},
    {"mp4a.6B", "MPEG"},
    {"ac-3", "AC3"},
    {"ec-3", "EAC3"},
    {"flac", "FLAC"}};

std::string GetCodecName(const std::string& codec) {
  auto* it =
      find_if(begin(kCodecList), end(kCodecList),
              [&codec](const std::pair<std::string, std::string>& codec_list) {
                return base::MatchPattern(codec, codec_list.first);
              });
  if (it == end(kCodecList))
    return "";
  return it->second;
}

}  // namespace

namespace media {

MediaPrefsInfo::MediaPrefsInfo() = default;

MediaPrefsInfo::MediaPrefsInfo(const MediaPrefsInfo& other) = default;

MediaPrefsInfo::~MediaPrefsInfo() = default;

std::string MediaPrefsInfo::ToString() const {
  std::ostringstream s;
  s << "mse_use_intrinsic_size: " << (mse_use_intrinsic_size ? "true" : "false")
    << " / is_av1_codec_enabled: " << (is_av1_codec_enabled ? "true" : "false")
    << " / is_supported_dolby_hdr: "
    << (is_supported_dolby_hdr ? "true" : "false")
    << " / is_supported_dolby_atmos: "
    << (is_supported_dolby_atmos ? "true" : "false");
  return s.str();
}

//  static
MediaPreferences* MediaPreferences::Get() {
  return base::Singleton<MediaPreferences>::get();
}

MediaPreferences::~MediaPreferences() = default;

std::string MediaPreferences::GetRawMediaPreferences() {
  base::AutoLock auto_lock(lock_);
  return raw_preferences;
}

bool MediaPreferences::HasValidCodecCapabilities() {
  base::AutoLock auto_lock(lock_);
  return !capabilities_.empty();
}

bool MediaPreferences::UseIntrinsicSizeForMSE() {
  base::AutoLock auto_lock(lock_);
  return media_prefs_info_.mse_use_intrinsic_size;
}

void MediaPreferences::SetUseIntrinsicSizeForMSE(bool use_intrinsic_size) {
  base::AutoLock auto_lock(lock_);
  media_prefs_info_.mse_use_intrinsic_size = use_intrinsic_size;
}

bool MediaPreferences::IsAV1CodecEnabled() {
  base::AutoLock auto_lock(lock_);
  return media_prefs_info_.is_av1_codec_enabled;
}

std::vector<MediaCodecCapability>
MediaPreferences::GetMediaCodecCapabilities() {
  base::AutoLock auto_lock(lock_);
  return capabilities_;
}

absl::optional<MediaCodecCapability>
MediaPreferences::GetMediaCodecCapabilityForCodec(const std::string& codec) {
  base::AutoLock auto_lock(lock_);
  std::string codec_name = ::GetCodecName(codec);
  auto it = std::find_if(capabilities_.begin(), capabilities_.end(),
                         [&codec_name](const MediaCodecCapability& capability) {
                           return codec_name == capability.codec;
                         });
  if (it != capabilities_.end())
    return *it;
  return absl::nullopt;
}

bool MediaPreferences::IsSupportedDolbyHdr() {
  base::AutoLock auto_lock(lock_);
  return media_prefs_info_.is_supported_dolby_hdr;
}

bool MediaPreferences::IsSupportedDolbyAtmos() {
  base::AutoLock auto_lock(lock_);
  return media_prefs_info_.is_supported_dolby_atmos;
}

}  // namespace media
