// Copyright 2016-2018 LG Electronics, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_MEDIA_INFO_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_MEDIA_INFO_LOADER_H_

#include "base/callback.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "url/gurl.h"

namespace blink {
class WebAssociatedURLLoader;
struct WebURLError;
class WebLocalFrame;
class WebURLLoader;
class WebURLResponse;

// This class is used to check if URL passes whitelisting
// filter implemented by webview. Similar to Android version
// but simplified approach.
class MediaInfoLoader : private WebAssociatedURLLoaderClient {
 public:
  typedef base::RepeatingCallback<void(bool, const GURL&)> ReadyCB;

  MediaInfoLoader(const GURL& url, const ReadyCB& ready_cb);
  ~MediaInfoLoader() override;

  void Start(WebLocalFrame* frame);

 private:
  // From WebURLLoaderClient
  bool WillFollowRedirect(const WebURL& new_url,
                          const WebURLResponse& redirectResponse) override;
  void DidReceiveResponse(/*WebURLLoader* loader,*/
                          const WebURLResponse& response) override;
  void DidFinishLoading() override;
  void DidFail(const WebURLError&) override;

  void DidBecomeReady(bool ok);

  // Keeps track of an active WebURLLoader and associated state.
  std::unique_ptr<WebAssociatedURLLoader> active_loader_;

  GURL url_;
  ReadyCB ready_cb_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_NEVA_MEDIA_INFO_LOADER_H_
