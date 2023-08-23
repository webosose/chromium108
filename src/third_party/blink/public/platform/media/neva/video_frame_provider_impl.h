// Copyright 2017 LG Electronics, Inc.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_VIDEO_FRAME_PROVIDER_IMPL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_VIDEO_FRAME_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/pipeline_metadata.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace content {
class RenderThreadImpl;
}  // namespace content

namespace blink {

class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;

// This class provides three types of VideoFrame
//  kHole    : Support video hole based media player
//  kBlack   : Alternative video frame when unavailable from media player
class BLINK_PLATFORM_EXPORT VideoFrameProviderImpl
    : public cc::VideoFrameProvider,
      public base::SupportsWeakPtr<VideoFrameProviderImpl> {
 public:
  enum FrameType { kUnknown = 0, kBlack, kHole };

  VideoFrameProviderImpl(const scoped_refptr<base::SingleThreadTaskRunner>&
                             compositor_task_runner);
  VideoFrameProviderImpl(const VideoFrameProviderImpl&) = delete;
  VideoFrameProviderImpl& operator=(const VideoFrameProviderImpl&) = delete;
  ~VideoFrameProviderImpl() override;

  // cc::VideoFrameProvider implementation.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;
  base::TimeDelta GetPreferredRenderInterval() override;
  void OnContextLost() override;

  // Getter method to |client_|.
  WebMediaPlayerClient* GetClient();
  void ActiveRegionChanged(const gfx::Rect&);
  void Repaint();
  void SetCurrentVideoFrame(scoped_refptr<media::VideoFrame> current_frame);
  base::Lock& GetLock();
  void SetWebLocalFrame(WebLocalFrame* frame);
  void SetWebMediaPlayerClient(WebMediaPlayerClient* client);
  void UpdateVideoFrame();
  gfx::Rect& GetActiveVideoRegion();
  gfx::Size GetNaturalVideoSize();
  void SetNaturalVideoSize(gfx::Size natural_video_size);
  void SetFrameType(FrameType type);
  void SetOverlayPlaneId(const base::UnguessableToken& overlay_plane_id);

 protected:
  scoped_refptr<media::VideoFrame> CreateVideoFrame(FrameType frame_type);

  WebLocalFrame* frame_;
  WebMediaPlayerClient* client_;
  bool is_suspended_;

  gfx::Size natural_video_size_;

  // Video frame rendering members.
  //
  // |lock_| protects |current_frame_| since new frames arrive on the video
  // rendering thread, yet are accessed for rendering on either the main thread
  // or compositing thread depending on whether accelerated compositing is used.
  base::Lock lock_;
  scoped_refptr<media::VideoFrame> current_frame_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  gfx::Rect active_video_region_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

 private:
  base::UnguessableToken overlay_plane_id_;
  FrameType frame_type_ = FrameType::kUnknown;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_NEVA_VIDEO_FRAME_PROVIDER_IMPL_H_
