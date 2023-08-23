// Copyright 2022 LG Electronics, Inc.
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

#ifndef MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_SERVICE_H_
#define MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_SERVICE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/neva/webos/luna_service_client.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MEDIA_EXPORT WebOSAudioService
    : public base::RefCountedThreadSafe<WebOSAudioService> {
 public:
  struct DeviceEntry {
    DeviceEntry(const std::string& name,
                const std::string& details,
                const std::string& id)
        : device_name(name), device_details(details), display_id(id) {}
    std::string device_name;
    std::string device_details;
    std::string display_id;
  };

  explicit WebOSAudioService();
  virtual ~WebOSAudioService();

  bool GetDeviceList(bool input, std::vector<DeviceEntry>* device_list);
  std::string RegisterTrack(const std::string& stream_type);
  bool UnregisterTrack(const std::string& track_id);

  bool SetTrackVolume(const std::string& track_id, int volume);
  bool SetSourceInputVolume(const std::string& stream_type, double volume);

  bool GetRootDictionary(const std::string& payload,
                         std::unique_ptr<base::DictionaryValue>* root);

  base::SingleThreadTaskRunner* GetTaskRunner() const {
    return luna_call_thread_.task_runner().get();
  }

 private:
  struct LunaCbHandle {
    LunaCbHandle(const std::string& uri,
                 const std::string& param,
                 std::string* response)
        : uri_(uri), param_(param), response_(response) {}
    std::string uri_;
    std::string param_;
    std::string* response_ = nullptr;
    base::WaitableEvent sync_done_;
  };

  friend class base::RefCountedThreadSafe<WebOSAudioService>;

  WebOSAudioService(const WebOSAudioService&) = delete;
  WebOSAudioService& operator=(const WebOSAudioService&) = delete;

  bool LunaCallInternal(const std::string& uri,
                        const std::string& param,
                        std::string* response);
  void OnLunaCallResponse(LunaCbHandle* handle, const std::string& response);

  std::unique_ptr<base::LunaServiceClient> luna_service_client_;

  base::Thread luna_call_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> luna_response_runner_ = nullptr;

  base::Lock audio_service_lock_;

  base::WeakPtr<WebOSAudioService> weak_this_;
  base::WeakPtrFactory<WebOSAudioService> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WEBOS_WEBOS_CAMERA_SERVICE_H_
