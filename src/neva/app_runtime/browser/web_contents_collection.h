// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/tab_contents/web_contents_collection.h

#ifndef NEVA_APP_RUNTIME_BROWSER_WEB_CONTENTS_COLLECTION_H_
#define NEVA_APP_RUNTIME_BROWSER_WEB_CONTENTS_COLLECTION_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace neva_app_runtime {

class WebContentsCollection {
 public:
  class Observer {
   public:
    virtual void WebContentsDestroyed(content::WebContents* web_contents) {}
    virtual void PrimaryMainFrameRenderProcessGone(
        content::WebContents* web_contents,
        base::TerminationStatus status) {}
    virtual void NavigationEntryCommitted(
        content::WebContents* web_contents,
        const content::LoadCommittedDetails& load_details) {}

   protected:
    virtual ~Observer() = default;
  };

  explicit WebContentsCollection(Observer* observer);
  ~WebContentsCollection();

  void StartObserving(content::WebContents* web_contents);
  void StopObserving(content::WebContents* web_contents);

 private:
  class ForwardingWebContentsObserver;

  void WebContentsDestroyed(content::WebContents* web_contents);

  Observer* const observer_;
  base::flat_map<content::WebContents*,
                 std::unique_ptr<ForwardingWebContentsObserver>>
      web_contents_observers_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_WEB_CONTENTS_COLLECTION_H_
