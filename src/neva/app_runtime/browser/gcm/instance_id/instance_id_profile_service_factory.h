// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h

#ifndef NEVA_APP_RUNTIME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace instance_id {

class InstanceIDProfileService;

// Singleton that owns all InstanceIDProfileService and associates them with
// profiles.
class InstanceIDProfileServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static InstanceIDProfileService* GetForProfile(
      content::BrowserContext* profile);
  static InstanceIDProfileServiceFactory* GetInstance();

  InstanceIDProfileServiceFactory(const InstanceIDProfileServiceFactory&) =
      delete;
  InstanceIDProfileServiceFactory& operator=(
      const InstanceIDProfileServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<InstanceIDProfileServiceFactory>;

  InstanceIDProfileServiceFactory();
  ~InstanceIDProfileServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace instance_id

#endif  // NEVA_APP_RUNTIME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
