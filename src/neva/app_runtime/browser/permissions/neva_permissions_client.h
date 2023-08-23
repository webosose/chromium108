// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/permissions/chrome_permissions_client.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_NEVA_PERMISSIONS_CLIENT_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_NEVA_PERMISSIONS_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/permissions/permissions_client.h"

namespace neva_app_runtime {

class NevaPermissionsClient : public permissions::PermissionsClient {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    virtual std::unique_ptr<permissions::PermissionPrompt> CreatePrompt(
        content::WebContents* web_contents,
        permissions::PermissionPrompt::Delegate* delegate) = 0;
  };

  NevaPermissionsClient(const NevaPermissionsClient&) = delete;
  NevaPermissionsClient& operator=(const NevaPermissionsClient&) = delete;

  static NevaPermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  bool IsSubresourceFilterActivated(content::BrowserContext* browser_context,
                                    const GURL& url) override;
  permissions::OriginKeyedPermissionActionService*
  GetOriginKeyedPermissionActionService(
      content::BrowserContext* browser_context) override;
  permissions::PermissionActionsHistory* GetPermissionActionsHistory(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  permissions::ObjectPermissionContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;

  bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                     const GURL& embedding_origin) override;
  std::unique_ptr<permissions::PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate* delegate) override;

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  friend base::NoDestructor<NevaPermissionsClient>;

  NevaPermissionsClient() = default;

  Delegate* delegate_ = nullptr;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_NEVA_PERMISSIONS_CLIENT_H_
