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

#include "neva/browser_service/browser/mediacapture_service_impl.h"

namespace browser {

// static
MediaCaptureServiceImpl* MediaCaptureServiceImpl::Get() {
  return base::Singleton<MediaCaptureServiceImpl>::get();
}

void MediaCaptureServiceImpl::AddBinding(
    mojo::PendingReceiver<mojom::MediaCaptureService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaCaptureServiceImpl::NotifyAudioCaptureState(bool state) {
  for (auto& listener : listeners_) {
    listener->NotifyAudioCaptureState(state);
  }
}

void MediaCaptureServiceImpl::NotifyVideoCaptureState(bool state) {
  for (auto& listener : listeners_) {
    listener->NotifyVideoCaptureState(state);
  }
}

void MediaCaptureServiceImpl::NotifyWindowCaptureState(bool state) {
  for (auto& listener : listeners_) {
    listener->NotifyWindowCaptureState(state);
  }
}

void MediaCaptureServiceImpl::NotifyDisplayCaptureState(bool state) {
  for (auto& listener : listeners_) {
    listener->NotifyDisplayCaptureState(state);
  }
}

void MediaCaptureServiceImpl::RegisterListener(
    RegisterListenerCallback callback) {
  mojo::AssociatedRemote<mojom::MediaCaptureListener> listener;
  std::move(callback).Run(listener.BindNewEndpointAndPassReceiver());
  listeners_.Add(std::move(listener));
}

}  // namespace browser