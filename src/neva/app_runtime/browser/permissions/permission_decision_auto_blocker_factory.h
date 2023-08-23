// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// weblayer/browser/permissions/permission_decision_auto_blocker_factory.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace neva_app_runtime {

class PermissionDecisionAutoBlockerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PermissionDecisionAutoBlockerFactory(
      const PermissionDecisionAutoBlockerFactory&) = delete;
  PermissionDecisionAutoBlockerFactory& operator=(
      const PermissionDecisionAutoBlockerFactory&) = delete;

  static permissions::PermissionDecisionAutoBlocker* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PermissionDecisionAutoBlockerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PermissionDecisionAutoBlockerFactory>;

  PermissionDecisionAutoBlockerFactory();
  ~PermissionDecisionAutoBlockerFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
