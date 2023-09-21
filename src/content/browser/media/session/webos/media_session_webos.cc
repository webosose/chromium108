// Copyright 2021 LG Electronics, Inc.
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

#include "content/browser/media/session/webos/media_session_webos.h"

#include <sstream>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/media_session.h"
#include "media/base/bind_to_current_loop.h"
#include "neva/logging.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace content {

namespace {

const char kAppId[] = "appId";
const char kMediaId[] = "mediaId";
const char kSubscribe[] = "subscribe";
const char kSubscribed[] = "subscribed";
const char kReturnValue[] = "returnValue";
const char kKeyEvent[] = "keyEvent";

const char kMediaMetaData[] = "mediaMetaData";
const char kMediaMetaDataTitle[] = "title";
const char kMediaMetaDataArtist[] = "artist";
const char kMediaMetaDataAlbum[] = "album";
const char kMediaMetaDataTotalDuration[] = "totalDuration";

const char kMediaPlayStatus[] = "playStatus";
const char kMediaPlayStatusStopped[] = "PLAYSTATE_STOPPED";
const char kMediaPlayStatusPaused[] = "PLAYSTATE_PAUSED";
const char kMediaPlayStatusPlaying[] = "PLAYSTATE_PLAYING";
const char kMediaPlayStatusNone[] = "PLAYSTATE_NONE";

const char kMediaPlayPosition[] = "playPosition";

const char kPlayEvent[] = "play";
const char kPauseEvent[] = "pause";
const char kNextEvent[] = "next";
const char kPreviousEvent[] = "previous";

const char kRegisterMediaSession[] = "registerMediaSession";
const char kUnregisterMediaSession[] = "unregisterMediaSession";
const char kActivateMediaSession[] = "activateMediaSession";
const char kDeactivateMediaSession[] = "deactivateMediaSession";
const char kSetMediaMetaData[] = "setMediaMetaData";
const char kSetMediaPlayStatus[] = "setMediaPlayStatus";
const char kSetMediaPlayPosition[] = "setMediaPlayPosition";

}  // namespace

#define BIND_TO_CURRENT_LOOP(function) \
  (media::BindToCurrentLoop(base::BindRepeating(function, weak_this_)))

MediaSessionWebOS::MediaSessionWebOS(MediaSessionImpl* session)
    : media_session_(session) {
  NEVA_DCHECK(media_session_);
  weak_this_ = weak_factory_.GetWeakPtr();

  content::WebContents* web_contents =
      static_cast<WebContentsImpl*>(session->web_contents());

  NEVA_DCHECK(web_contents);
  blink::RendererPreferences* renderer_prefs =
      web_contents->GetMutableRendererPrefs();

  NEVA_DCHECK(renderer_prefs);
  luna_service_client_.reset(
      new base::LunaServiceClient(renderer_prefs->application_id));

  application_id_ = renderer_prefs->application_id + renderer_prefs->display_id;

  NEVA_VLOGTF(1) << " Application id: " << application_id_;

  session->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
}

MediaSessionWebOS::~MediaSessionWebOS() {
  NEVA_VLOGTF(1);
  UnregisterMediaSession();
}

void MediaSessionWebOS::MediaSessionRequestChanged(
    const absl::optional<base::UnguessableToken>& request_id) {
  NEVA_VLOGTF(1);

  if (!session_id_.empty()) {
    // Previous session is active. Deactivate it.
    UnregisterMediaSession();
  }

  if (!request_id.has_value()) {
    NEVA_LOGF(ERROR) << " Session id is not received";
    return;
  }

  if (!RegisterMediaSession(request_id->ToString())) {
    NEVA_LOGF(ERROR) << " Register session failed for "
                     << request_id->ToString();
    return;
  }

  if (!ActivateMediaSession(request_id->ToString())) {
    NEVA_LOGF(ERROR) << " Activate session failed for "
                     << request_id->ToString();
    return;
  }

  session_id_ = request_id->ToString();
}

void MediaSessionWebOS::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  NEVA_VLOGTF(1) << ", playback_state: " << session_info->playback_state;
  if (session_id_.empty() || mcs_permission_error_)
    return;

  PlaybackState playback_state =
      ConvertIntoWebOSPlaybackState(session_info->playback_state);
  if (playback_state_ == playback_state)
    return;

  SetPlaybackStatusInternal(playback_state_);
}

void MediaSessionWebOS::MediaSessionMetadataChanged(
    const absl::optional<media_session::MediaMetadata>& metadata) {
  NEVA_VLOGTF(1);
  if (session_id_.empty() || mcs_permission_error_)
    return;

  if (!metadata.has_value()) {
    NEVA_LOGF(ERROR) << " Metadata is not received";
    return;
  }

  if (!metadata->title.empty())
    SetMetadataPropertyInternal(kMediaMetaDataTitle, metadata->title);

  if (!metadata->artist.empty())
    SetMetadataPropertyInternal(kMediaMetaDataArtist, metadata->artist);

  if (!metadata->album.empty())
    SetMetadataPropertyInternal(kMediaMetaDataAlbum, metadata->album);
}

void MediaSessionWebOS::MediaSessionPositionChanged(
    const absl::optional<media_session::MediaPosition>& position) {
  if (session_id_.empty() || mcs_permission_error_)
    return;

  if (!position.has_value()) {
    NEVA_LOGF(ERROR) << " position is not received";
    return;
  }

  SetMediaPositionInternal(position->GetPosition());

  // Send duration
  base::TimeDelta new_duration = position->duration();
  if (duration_ == new_duration)
    return;

  duration_ = new_duration;
  SetMetadataPropertyInternal(kMediaMetaDataTotalDuration,
                              base::NumberToString16(duration_.InSecondsF()));
}

bool MediaSessionWebOS::RegisterMediaSession(const std::string& session_id) {
  if (session_id.empty()) {
    NEVA_LOGF(ERROR) << " Invalid session id";
    return false;
  }

  base::DictionaryValue root_dict;
  root_dict.SetKey(kMediaId, base::Value(session_id));
  root_dict.SetKey(kAppId, base::Value(application_id_));
  root_dict.SetKey(kSubscribe, base::Value(true));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write registMediaSession payload";
    return false;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->Subscribe(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kRegisterMediaSession),
      payload, &subscribe_key_,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::ReceiveMediaKeyEvent));

  registered_ = true;
  return true;
}

void MediaSessionWebOS::UnregisterMediaSession() {
  if (!registered_) {
    NEVA_LOGF(ERROR) << " Session is already unregistered";
    return;
  }

  if (session_id_.empty()) {
    NEVA_LOGF(ERROR) << " No registered session";
    return;
  }

  if (playback_state_ != PlaybackState::kStopped)
    SetPlaybackStatusInternal(PlaybackState::kStopped);

  base::DictionaryValue root_dict;
  root_dict.SetKey(kMediaId, base::Value(session_id_));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write unregistMediaSession payload";
    return;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kUnregisterMediaSession),
      payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));

  luna_service_client_->Unsubscribe(subscribe_key_);

  registered_ = false;
  session_id_ = std::string();
  duration_ = base::TimeDelta();
}

bool MediaSessionWebOS::ActivateMediaSession(const std::string& session_id) {
  if (session_id.empty()) {
    NEVA_LOGF(ERROR) << " Invalid session id";
    return false;
  }

  base::DictionaryValue root_dict;
  root_dict.SetKey(kMediaId, base::Value(session_id));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write activateMediaSession payload";
    return false;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kActivateMediaSession),
      payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));

  return true;
}

void MediaSessionWebOS::DeactivateMediaSession() {
  if (session_id_.empty()) {
    NEVA_LOGF(ERROR) << " No active session";
    return;
  }

  base::DictionaryValue root_dict;
  root_dict.SetKey(kMediaId, base::Value(session_id_));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write deactivateMediaSession payload";
    return;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kDeactivateMediaSession),
      payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));
}

void MediaSessionWebOS::SetPlaybackStatusInternal(
    PlaybackState playback_state) {
  if (session_id_.empty()) {
    NEVA_LOGF(ERROR) << " No active session";
    return;
  }

  if (playback_state_ == playback_state)
    return;

  static std::map<PlaybackState, std::string> kPlaybackStateMap = {
      {PlaybackState::kPaused, kMediaPlayStatusPaused},
      {PlaybackState::kPlaying, kMediaPlayStatusPlaying},
      {PlaybackState::kStopped, kMediaPlayStatusStopped}};

  auto get_playback_status = [&](PlaybackState status) {
    auto it = kPlaybackStateMap.find(status);
    if (it != kPlaybackStateMap.end())
      return it->second;
    return std::string(kMediaPlayStatusNone);
  };

  playback_state_ = playback_state;
  std::string play_status = get_playback_status(playback_state_);

  base::DictionaryValue root_dict;
  root_dict.SetKey(kMediaId, base::Value(session_id_));
  root_dict.SetKey(kMediaPlayStatus, base::Value(play_status));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write setMediaPlayStatus payload";
    return;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kSetMediaPlayStatus),
      payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));
}

void MediaSessionWebOS::SetMediaPositionInternal(
    const base::TimeDelta& position) {
  if (session_id_.empty()) {
    LOG(ERROR) << __func__ << " No active session.";
    return;
  }

  base::DictionaryValue playposition_root;
  playposition_root.SetStringKey(kMediaId, session_id_);
  playposition_root.SetStringKey(kMediaPlayPosition,
                                 std::to_string(position.InSecondsF()));

  std::string playposition_payload;
  if (!base::JSONWriter::Write(playposition_root, &playposition_payload)) {
    LOG(ERROR) << __func__ << " Failed to write Play Position payload";
    return;
  }
  VLOG(1) << __func__ << " playposition_payload: " << playposition_payload;

  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER,
          kSetMediaPlayPosition),
      playposition_payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));
}

void MediaSessionWebOS::SetMetadataPropertyInternal(
    const std::string& property,
    const std::u16string& value) {
  base::DictionaryValue metadata;
  metadata.SetStringKey(property, base::UTF16ToUTF8(value));

  base::DictionaryValue root_dict;
  root_dict.SetStringKey(kMediaId, session_id_);
  root_dict.SetKey(kMediaMetaData, std::move(metadata));

  std::string payload;
  if (!base::JSONWriter::Write(root_dict, &payload)) {
    NEVA_LOGF(ERROR) << " Failed to write setMediaMetaData payload";
    return;
  }

  NEVA_VLOGTF(1) << " payload: " << payload;
  luna_service_client_->CallAsync(
      base::LunaServiceClient::GetServiceURI(
          base::LunaServiceClient::URIType::MEDIACONTROLLER, kSetMediaMetaData),
      payload,
      BIND_TO_CURRENT_LOOP(&MediaSessionWebOS::CheckReplyStatusMessage));
}

void MediaSessionWebOS::ReceiveMediaKeyEvent(const std::string& payload) {
  NEVA_VLOGTF(1) << " payload: " << payload;

  if (mcs_permission_error_)
    return;

  absl::optional<base::Value> value = base::JSONReader::Read(payload);
  if (!value) {
    return;
  }

  auto response = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*value)));

  auto return_value = response->FindBoolPath(kReturnValue);
  auto subscribed = response->FindBoolPath(kSubscribed);

  if (!return_value || !*return_value || !subscribed || !*subscribed) {
    mcs_permission_error_ = true;
    NEVA_LOGF(ERROR) << " Failed to Register with MCS"
                     << ", session_id: " << session_id_
                     << ", mcs_permission_error: " << mcs_permission_error_;
    return;
  }

  const std::string* session_id = response->FindStringPath(kMediaId);

  if (!session_id || session_id->empty()) {
    NEVA_LOGF(WARNING) << " Session ID is not sent";
    return;
  }

  if (session_id_ != *session_id) {
    NEVA_VLOGTF(1) << " Event recieved for other session. Ignore.";
    return;
  }

  const std::string* key_event = response->FindStringPath(kKeyEvent);
  if (key_event) {
    HandleMediaKeyEvent(key_event);
  } else {
    NEVA_VLOGTF(1) << " Successfully Registered with MCS, session_id: "
                   << session_id_;
  }
}

void MediaSessionWebOS::CheckReplyStatusMessage(const std::string& message) {
  NEVA_VLOGTF(1) << " message: " << message;

  if (mcs_permission_error_)
    return;

  absl::optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    return;
  }

  auto response = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(*value)));

  auto return_value = response->FindBoolPath(kReturnValue);
  if (!return_value || !*return_value) {
    NEVA_LOGF(ERROR) << " MCS call Failed. message: " << message
                     << " session_id: " << session_id_;
    return;
  }

  NEVA_VLOGTF(1) << " MCS call Success. message: " << message
                 << " session_id: " << session_id_;
}

void MediaSessionWebOS::HandleMediaKeyEvent(const std::string* key_event) {
  NEVA_VLOGTF(1) << " key_event: " << *key_event;

  static std::map<std::string, MediaKeyEvent> kEventKeyMap = {
      {kPlayEvent, MediaSessionWebOS::MediaKeyEvent::kPlay},
      {kPauseEvent, MediaSessionWebOS::MediaKeyEvent::kPause},
      {kNextEvent, MediaSessionWebOS::MediaKeyEvent::kNext},
      {kPreviousEvent, MediaSessionWebOS::MediaKeyEvent::kPrevious}};

  auto get_event_type = [&](const std::string* key) {
    std::map<std::string, MediaKeyEvent>::iterator it;
    it = kEventKeyMap.find(*key);
    if (it != kEventKeyMap.end())
      return it->second;
    return MediaSessionWebOS::MediaKeyEvent::kUnsupported;
  };

  if (media_session_) {
    MediaKeyEvent event_type = get_event_type(key_event);
    switch (event_type) {
      case MediaSessionWebOS::MediaKeyEvent::kPlay:
        media_session_->Resume(MediaSession::SuspendType::kUI);
        break;
      case MediaSessionWebOS::MediaKeyEvent::kPause:
        media_session_->Suspend(MediaSession::SuspendType::kUI);
        break;
      case MediaSessionWebOS::MediaKeyEvent::kNext:
        media_session_->NextTrack();
        break;
      case MediaSessionWebOS::MediaKeyEvent::kPrevious:
        media_session_->PreviousTrack();
        break;
      default:
        NOTREACHED() << " key_event: " << key_event << " Not Handled !!!";
        break;
    }
  }
}

MediaSessionWebOS::PlaybackState
MediaSessionWebOS::ConvertIntoWebOSPlaybackState(
    media_session::mojom::MediaPlaybackState mojom_state) {
  switch (mojom_state) {
    case media_session::mojom::MediaPlaybackState::kPaused:
      return PlaybackState::kPaused;
    case media_session::mojom::MediaPlaybackState::kPlaying:
      return PlaybackState::kPlaying;
    case media_session::mojom::MediaPlaybackState::kStopped:
      return PlaybackState::kStopped;
    default:
      return PlaybackState::kUnknown;
  }
}

}  // namespace content
