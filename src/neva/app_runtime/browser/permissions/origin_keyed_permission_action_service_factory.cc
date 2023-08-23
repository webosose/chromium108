// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/permissions/origin_keyed_permission_action_service_factory.cc

#include "neva/app_runtime/browser/permissions/origin_keyed_permission_action_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/origin_keyed_permission_action_service.h"

// static
permissions::OriginKeyedPermissionActionService*
OriginKeyedPermissionActionServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<permissions::OriginKeyedPermissionActionService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
OriginKeyedPermissionActionServiceFactory*
OriginKeyedPermissionActionServiceFactory::GetInstance() {
  return base::Singleton<OriginKeyedPermissionActionServiceFactory>::get();
}

OriginKeyedPermissionActionServiceFactory::
    OriginKeyedPermissionActionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginKeyedPermissionActionService",
          BrowserContextDependencyManager::GetInstance()) {}

OriginKeyedPermissionActionServiceFactory::
    ~OriginKeyedPermissionActionServiceFactory() = default;

KeyedService*
OriginKeyedPermissionActionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::OriginKeyedPermissionActionService();
}
