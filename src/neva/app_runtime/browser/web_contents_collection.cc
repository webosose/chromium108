// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/tab_contents/web_contents_collection.cc

#include "neva/app_runtime/browser/web_contents_collection.h"

#include "base/check.h"
#include "content/public/browser/web_contents_observer.h"

namespace neva_app_runtime {

class WebContentsCollection::ForwardingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ForwardingWebContentsObserver(content::WebContents* web_contents,
                                WebContentsCollection::Observer* observer,
                                WebContentsCollection* collection)
      : content::WebContentsObserver(web_contents),
        observer_(observer),
        collection_(collection) {}

 private:
  // WebContentsObserver:
  void WebContentsDestroyed() override {
    collection_->WebContentsDestroyed(web_contents());
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    observer_->PrimaryMainFrameRenderProcessGone(web_contents(), status);
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    observer_->NavigationEntryCommitted(web_contents(), load_details);
  }

  WebContentsCollection::Observer* observer_;
  WebContentsCollection* collection_;
};

WebContentsCollection::WebContentsCollection(
    WebContentsCollection::Observer* observer)
    : observer_(observer) {}

WebContentsCollection::~WebContentsCollection() = default;

void WebContentsCollection::StartObserving(content::WebContents* web_contents) {
  if (web_contents_observers_.find(web_contents) !=
      web_contents_observers_.end())
    return;

  auto emplace_result = web_contents_observers_.emplace(
      web_contents, std::make_unique<ForwardingWebContentsObserver>(
                        web_contents, observer_, this));
  DCHECK(emplace_result.second);
}

void WebContentsCollection::StopObserving(content::WebContents* web_contents) {
  web_contents_observers_.erase(web_contents);
}

void WebContentsCollection::WebContentsDestroyed(
    content::WebContents* web_contents) {
  web_contents_observers_.erase(web_contents);
  observer_->WebContentsDestroyed(web_contents);
}

}  // namespace neva_app_runtime
