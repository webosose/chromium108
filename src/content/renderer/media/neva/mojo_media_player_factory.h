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

#ifndef CONTENT_RENDERER_MEDIA_NEVA_MOJO_MEDIA_PLAYER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_NEVA_MOJO_MEDIA_PLAYER_FACTORY_H_

#include "media/neva/media_player_neva_factory.h"

namespace media {

MediaPlayerNeva* CreateMojoMediaPlayer(
    MediaPlayerNevaClient*,
    MediaPlayerType,
    const scoped_refptr<base::SingleThreadTaskRunner>&,
    const std::string& app_id);

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_NEVA_MOJO_MEDIA_PLAYER_FACTORY_H_
