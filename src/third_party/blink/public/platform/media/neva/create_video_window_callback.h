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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_CREATE_VIDEO_WINDOW_CALLBACK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_CREATE_VIDEO_WINDOW_CALLBACK_H_

#include "ui/platform_window/neva/mojom/video_window.mojom.h"

namespace blink {

using CreateVideoWindowCallback = base::RepeatingCallback<void(
    mojo::PendingRemote<ui::mojom::VideoWindowClient>,
    mojo::PendingReceiver<ui::mojom::VideoWindow>,
    const ui::VideoWindowParams&)>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_CREATE_VIDEO_WINDOW_CALLBACK_H_
