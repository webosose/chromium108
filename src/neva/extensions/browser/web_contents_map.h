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

#ifndef NEVA_EXTENSIONS_BROWSER_WEB_CONTENTS_MAP_H
#define NEVA_EXTENSIONS_BROWSER_WEB_CONTENTS_MAP_H

#include "content/public/browser/web_contents.h"

#include <set>

#include "base/memory/singleton.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "url/gurl.h"

namespace neva {

class WebContentsMap;

class WebContentsItem : public extensions::ExtensionWebContentsObserver {
 public:
  WebContentsItem(WebContentsMap* web_contents_map,
                  content::WebContents* web_contents);
  ~WebContentsItem() override = default;

  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  void WebContentsDestroyed() override;

 private:
  WebContentsMap* web_contents_map_;
};

class WebContentsMap {
 public:
  static WebContentsMap* GetInstance();

  void OnWebContentsCreated(content::WebContents* web_contents);

  void OnWebContentsWillDestroyed(WebContentsItem* item);

  extensions::ExtensionWebContentsObserver* GetObserver(
      content::WebContents* web_contents);

  void SetTabIdForRenderFrame(content::RenderFrameHost* host);

 private:
  friend struct base::DefaultSingletonTraits<WebContentsMap>;
  WebContentsMap();
  ~WebContentsMap();

  std::map<WebContentsItem*, content::WebContents*> items_;
};

}  // namespace neva

#endif  // NEVA_EXTENSIONS_BROWSER_WEB_CONTENTS_MAP_H
