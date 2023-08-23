// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/permissions/permission_manager_factory.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/permissions/permission_manager.h"

namespace content {
class BrowserContext;
}

namespace permissions {
class PermissionManager;
}

class PermissionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static permissions::PermissionManager* GetForBrowserContext(
      content::BrowserContext* context);
  static PermissionManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PermissionManagerFactory>;

  PermissionManagerFactory();
  ~PermissionManagerFactory() override;

  PermissionManagerFactory(const PermissionManagerFactory&) = delete;
  PermissionManagerFactory& operator=(const PermissionManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
