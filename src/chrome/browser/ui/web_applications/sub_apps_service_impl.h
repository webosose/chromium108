// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_

#include "chrome/browser/web_applications/web_app_id.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace web_app {

class SubAppsServiceImpl
    : public content::DocumentService<blink::mojom::SubAppsService> {
 public:
  SubAppsServiceImpl(const SubAppsServiceImpl&) = delete;
  SubAppsServiceImpl& operator=(const SubAppsServiceImpl&) = delete;
  ~SubAppsServiceImpl() override;

  // We only want to create this object when the Browser* associated with the
  // WebContents is an installed web app and when the RFH is the main frame.
  static void CreateIfAllowed(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::SubAppsService> receiver);

  // blink::mojom::SubAppsService
  void Add(std::vector<blink::mojom::SubAppsServiceAddInfoPtr> sub_apps,
           AddCallback result_callback) override;

  void List(ListCallback result_callback) override;
  void Remove(const UnhashedAppId& unhashed_app_id,
              RemoveCallback result_callback) override;

 private:
  SubAppsServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::SubAppsService> receiver);
  base::WeakPtrFactory<SubAppsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_
