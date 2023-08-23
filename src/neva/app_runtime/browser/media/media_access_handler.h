// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Reused from chrome/browser/media/media_access_handler.h

#ifndef NEVA_APP_RUNTIME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_
#define NEVA_APP_RUNTIME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_

#include "base/callback.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/media_stream_request.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace neva_app_runtime {

class MediaAccessHandler {
 public:
  MediaAccessHandler() = default;
  virtual ~MediaAccessHandler() = default;

  virtual bool SupportsStreamType(content::WebContents* web_contents,
                                  const blink::mojom::MediaStreamType type) = 0;

  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
      blink::mojom::MediaStreamType type) = 0;

  virtual void HandleRequest(content::WebContents* web_contents,
                             const content::MediaStreamRequest& request,
                             content::MediaResponseCallback callback) = 0;

  virtual void UpdateMediaRequestState(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      blink::mojom::MediaStreamType stream_type,
      content::MediaRequestState state) {}

  virtual bool IsInsecureCapturingInProgress(int render_process_id,
                                             int render_frame_id);

  virtual void UpdateVideoScreenCaptureStatus(int render_process_id,
                                              int render_frame_id,
                                              int page_request_id,
                                              bool is_secure) {}
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_
