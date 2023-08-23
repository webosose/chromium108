// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied from
// chrome/browser/gcm/instance_id/instance_id_profile_service_factory.cc

#include "neva/app_runtime/browser/gcm/instance_id/instance_id_profile_service_factory.h"

#include <memory>

#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/gcm/gcm_profile_service_factory.h"

namespace instance_id {

// static
InstanceIDProfileService* InstanceIDProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // Instance ID is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return NULL;

  return static_cast<InstanceIDProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstanceIDProfileServiceFactory*
InstanceIDProfileServiceFactory::GetInstance() {
  return base::Singleton<InstanceIDProfileServiceFactory>::get();
}

InstanceIDProfileServiceFactory::InstanceIDProfileServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "InstanceIDProfileService",
          BrowserContextDependencyManager::GetInstance()) {
  // GCM is needed for device ID.
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

InstanceIDProfileServiceFactory::~InstanceIDProfileServiceFactory() {}

KeyedService* InstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new InstanceIDProfileService(
      gcm::GCMProfileServiceFactory::GetForProfile(context)->driver(),
      context->IsOffTheRecord());
}

}  // namespace instance_id
