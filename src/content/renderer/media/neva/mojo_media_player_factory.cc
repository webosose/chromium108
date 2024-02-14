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

#include "content/renderer/media/neva/mojo_media_player_factory.h"

#include "content/child/child_thread_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "neva/neva_media_service/clients/mojo_media_player.h"

namespace media {

MediaPlayerNeva* CreateMojoMediaPlayer(
    MediaPlayerNevaClient* client,
    MediaPlayerType media_player_type,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const std::string& app_id) {
  mojo::PendingRemote<neva_media::mojom::MediaServiceProvider> pending_provider;
  content::ChildThreadImpl::current()->BindHostReceiver(
      pending_provider.InitWithNewPipeAndPassReceiver());
  return new neva_media::MojoMediaPlayer(std::move(pending_provider), client,
                                         media_player_type, main_task_runner,
                                         app_id);
}

}  // namespace media
