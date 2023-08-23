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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_MEDIACAPTURE_SERVICE_IMPL_H_
#define NEVA_BROWSER_SERVICE_BROWSER_MEDIACAPTURE_SERVICE_IMPL_H_

#include "base/memory/singleton.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "neva/browser_service/public/mojom/mediacapture_service.mojom.h"
#include "url/gurl.h"

namespace browser {

class MediaCaptureServiceImpl : public mojom::MediaCaptureService {
 public:
  static MediaCaptureServiceImpl* Get();
  // mojom::MediaCaptureListener
  void NotifyAudioCaptureState(bool state);
  void NotifyVideoCaptureState(bool state);
  void NotifyWindowCaptureState(bool state);
  void NotifyDisplayCaptureState(bool state);

  void AddBinding(mojo::PendingReceiver<mojom::MediaCaptureService> receiver);
  // mojom::MediaCaptureService
  void RegisterListener(RegisterListenerCallback callback) override;

 private:
  friend struct base::DefaultSingletonTraits<MediaCaptureServiceImpl>;

  MediaCaptureServiceImpl() = default;
  MediaCaptureServiceImpl(const MediaCaptureServiceImpl&) = delete;
  MediaCaptureServiceImpl& operator=(const MediaCaptureServiceImpl&) = delete;
  ~MediaCaptureServiceImpl() = default;

  mojo::AssociatedRemoteSet<mojom::MediaCaptureListener> listeners_;
  mojo::ReceiverSet<mojom::MediaCaptureService> receivers_;
};

}  // namespace browser

#endif  // NEVA_BROWSER_SERVICE_BROWSER_MEDIACAPTURE_SERVICE_IMPL_H_