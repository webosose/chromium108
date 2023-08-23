// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Referred from
// chrome/browser/media/webrtc/permission_bubble_media_access_handler.cc

#include "neva/app_runtime/browser/media/webrtc/device_media_stream_access_handler.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permissions_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "neva/app_runtime/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "neva/app_runtime/browser/media/webrtc/media_stream_capture_indicator.h"
#include "neva/app_runtime/browser/permissions/permission_manager_factory.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace neva_app_runtime {

namespace {

const char kAudioCaptureAllowed[] = "hardware.audio_capture_enabled";
const char kVideoCaptureAllowed[] = "hardware.video_capture_enabled";

const char kDefaultAudioCaptureDevice[] = "media.default_audio_capture_device";
const char kDefaultVideoCaptureDevice[] = "media.default_video_capture_Device";

using MediaResponseCallback =
    base::OnceCallback<void(const blink::MediaStreamDevices& devices,
                            blink::mojom::MediaStreamRequestResult result,
                            std::unique_ptr<content::MediaStreamUI> ui)>;

void UpdatePageSpecificContentSettings(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  if (!web_contents)
    return;

  content::RenderFrameHost* const render_frame_host =
      content::RenderFrameHost::FromID(request.render_process_id,
                                       request.render_frame_id);
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          render_frame_host);
  if (!content_settings)
    return;

  content_settings::PageSpecificContentSettings::MicrophoneCameraState
      microphone_camera_state = content_settings::PageSpecificContentSettings::
          MICROPHONE_CAMERA_NOT_ACCESSED;
  std::string selected_audio_device;
  std::string selected_video_device;
  std::string requested_audio_device = request.requested_audio_device_id;
  std::string requested_video_device = request.requested_video_device_id;

  content::BrowserContext* context = web_contents->GetBrowserContext();
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  if (audio_setting != CONTENT_SETTING_DEFAULT) {
    selected_audio_device = requested_audio_device.empty()
                                ? prefs->GetString(kDefaultAudioCaptureDevice)
                                : requested_audio_device;
    microphone_camera_state |=
        content_settings::PageSpecificContentSettings::MICROPHONE_ACCESSED |
        (audio_setting == CONTENT_SETTING_ALLOW
             ? 0
             : content_settings::PageSpecificContentSettings::
                   MICROPHONE_BLOCKED);
  }

  if (video_setting != CONTENT_SETTING_DEFAULT) {
    selected_video_device = requested_video_device.empty()
                                ? prefs->GetString(kDefaultVideoCaptureDevice)
                                : requested_video_device;
    microphone_camera_state |=
        content_settings::PageSpecificContentSettings::CAMERA_ACCESSED |
        (video_setting == CONTENT_SETTING_ALLOW
             ? 0
             : content_settings::PageSpecificContentSettings::CAMERA_BLOCKED);
  }

  GURL embedding_origin;
  if (permissions::PermissionsClient::Get()->DoURLsMatchNewTabPage(
          request.security_origin,
          web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL())) {
    embedding_origin =
        web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  } else {
    embedding_origin = permissions::PermissionUtil::GetLastCommittedOriginAsURL(
        render_frame_host->GetMainFrame());
  }

  content_settings->OnMediaStreamPermissionSet(
      permissions::PermissionUtil::GetCanonicalOrigin(
          ContentSettingsType::MEDIASTREAM_CAMERA, request.security_origin,
          embedding_origin),
      microphone_camera_state, selected_audio_device, selected_video_device,
      requested_audio_device, requested_video_device);
}

}  // namespace

struct DeviceMediaStreamAccessHandler::PendingAccessRequest {
  PendingAccessRequest(const content::MediaStreamRequest& request,
                       content::MediaResponseCallback callback)
      : request(request), callback(std::move(callback)) {}
  content::MediaStreamRequest request;
  content::MediaResponseCallback callback;
};

DeviceMediaStreamAccessHandler::DeviceMediaStreamAccessHandler()
    : web_contents_collection_(this) {}

DeviceMediaStreamAccessHandler::~DeviceMediaStreamAccessHandler() = default;

bool DeviceMediaStreamAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type) {
  return type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
}

bool DeviceMediaStreamAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  blink::PermissionType permission_type =
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? blink::PermissionType::AUDIO_CAPTURE
          : blink::PermissionType::VIDEO_CAPTURE;

  // TODO(crbug.com/1321100): Remove `security_origin`.
  if (render_frame_host->GetLastCommittedOrigin().GetURL() != security_origin) {
    return false;
  }
  // It is OK to ignore `security_origin` because it will be calculated from
  // `render_frame_host` and we always ignore `requesting_origin` for
  // `AUDIO_CAPTURE` and `VIDEO_CAPTURE`.
  // `render_frame_host->GetMainFrame()->GetLastCommittedOrigin()` will be used
  // instead.
  return render_frame_host->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(permission_type,
                                                     render_frame_host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void DeviceMediaStreamAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsMap& requests_map = pending_requests_[web_contents];
  requests_map.emplace(next_request_id_++,
                       PendingAccessRequest(request, std::move(callback)));

  // If this is the only request then show the infobar.
  if (requests_map.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void DeviceMediaStreamAccessHandler::ProcessQueuedAccessRequest(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = pending_requests_.find(web_contents);

  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was closed.
    return;
  }

  DCHECK(!it->second.empty());

  const int64_t request_id = it->second.begin()->first;
  const content::MediaStreamRequest& request =
      it->second.begin()->second.request;

  webrtc::MediaStreamDevicesController::RequestPermissions(
      request, MediaCaptureDevicesDispatcher::GetInstance(),
      base::BindOnce(
          &DeviceMediaStreamAccessHandler::OnMediaStreamRequestResponse,
          base::Unretained(this), web_contents, request_id, request));
}

void DeviceMediaStreamAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (state != content::MEDIA_REQUEST_STATE_CLOSING)
    return;

  bool found = false;
  for (auto requests_it = pending_requests_.begin();
       requests_it != pending_requests_.end(); ++requests_it) {
    RequestsMap& requests_map = requests_it->second;
    for (RequestsMap::iterator it = requests_map.begin();
         it != requests_map.end(); ++it) {
      if (it->second.request.render_process_id == render_process_id &&
          it->second.request.render_frame_id == render_frame_id &&
          it->second.request.page_request_id == page_request_id) {
        requests_map.erase(it);
        found = true;
        break;
      }
    }
    if (found)
      break;
  }
}

// static
void DeviceMediaStreamAccessHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(kVideoCaptureAllowed, false);
  prefs->RegisterBooleanPref(kAudioCaptureAllowed, false);
}

void DeviceMediaStreamAccessHandler::OnMediaStreamRequestResponse(
    content::WebContents* web_contents,
    int64_t request_id,
    content::MediaStreamRequest request,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    bool blocked_by_permissions_policy,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  if (pending_requests_.find(web_contents) == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  if (result != blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON &&
      !blocked_by_permissions_policy) {
    UpdatePageSpecificContentSettings(web_contents, request, audio_setting,
                                      video_setting);
  }

  blink::mojom::StreamDevices devices;
  if (!stream_devices_set.stream_devices.empty()) {
    devices = *stream_devices_set.stream_devices[0];
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (!stream_devices_set.stream_devices.empty() &&
      (stream_devices_set.stream_devices[0]->audio_device.has_value() ||
       stream_devices_set.stream_devices[0]->video_device.has_value())) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }
  OnAccessRequestResponse(web_contents, request_id, stream_devices_set, result,
                          std::move(ui));
}

void DeviceMediaStreamAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    int64_t request_id,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto request_maps_it = pending_requests_.find(web_contents);
  if (request_maps_it == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  RequestsMap& requests_map(request_maps_it->second);
  if (requests_map.empty())
    return;

  auto request_it = requests_map.find(request_id);
  DCHECK(request_it != requests_map.end());
  if (request_it == requests_map.end())
    return;

  blink::mojom::MediaStreamRequestResult final_result = result;

  content::MediaResponseCallback callback =
      std::move(request_it->second.callback);
  requests_map.erase(request_it);

  if (!requests_map.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DeviceMediaStreamAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), web_contents));
  }

  std::move(callback).Run(stream_devices_set, final_result, std::move(ui));
}

void DeviceMediaStreamAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}

}  // namespace neva_app_runtime
