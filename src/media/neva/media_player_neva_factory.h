// Copyright 2017-2020 LG Electronics, Inc.
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

#ifndef MEDIA_NEVA_MEDIA_PLAYER_NEVA_FACTORY_H_
#define MEDIA_NEVA_MEDIA_PLAYER_NEVA_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/media_export.h"
#include "media/neva/media_player_neva_types.h"

namespace media {
class MediaPlayerNeva;
class MediaPlayerNevaClient;

class MEDIA_EXPORT MediaPlayerNevaFactory {
 public:
  static MediaPlayerType GetMediaPlayerType(const std::string& mime_type);
  static MediaPlayerNeva* CreateMediaPlayerNeva(
      MediaPlayerNevaClient*,
      MediaPlayerType,
      const scoped_refptr<base::SingleThreadTaskRunner>&,
      const std::string& app_id);
};

using CreateMediaPlayerNevaCB = base::RepeatingCallback<decltype(
    MediaPlayerNevaFactory::CreateMediaPlayerNeva)>;

}  // namespace media

#endif  // MEDIA_NEVA_MEDIA_PLAYER_NEVA_FACTORY_H_
