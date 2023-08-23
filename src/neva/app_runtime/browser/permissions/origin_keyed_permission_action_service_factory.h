// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/permissions/origin_keyed_permission_action_service_factory.h
#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class OriginKeyedPermissionActionService;
}
// Factory to create a service to keep track of permission actions of the
// current browser session for metrics evaluation.
class OriginKeyedPermissionActionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  OriginKeyedPermissionActionServiceFactory(
      const OriginKeyedPermissionActionServiceFactory&) = delete;
  OriginKeyedPermissionActionServiceFactory& operator=(
      const OriginKeyedPermissionActionServiceFactory&) = delete;

  static permissions::OriginKeyedPermissionActionService* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static OriginKeyedPermissionActionServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      OriginKeyedPermissionActionServiceFactory>;
  OriginKeyedPermissionActionServiceFactory();

  ~OriginKeyedPermissionActionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_FACTORY_H_
