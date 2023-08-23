// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on chrome/browser/push_messaging/push_messaging_service_factory.cc
// Removed not necessary DependsOn

#include "neva/app_runtime/browser/push_messaging/push_messaging_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/gcm/gcm_profile_service_factory.h"
#include "neva/app_runtime/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_service_impl.h"

namespace neva_app_runtime {
// static
PushMessagingServiceImpl* PushMessagingServiceFactory::GetForProfile(
    content::BrowserContext* context) {
  // The Push API is not currently supported in incognito mode.
  // See https://crbug.com/401439.
  if (context->IsOffTheRecord())
    return nullptr;

  return static_cast<PushMessagingServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PushMessagingServiceFactory* PushMessagingServiceFactory::GetInstance() {
  return base::Singleton<PushMessagingServiceFactory>::get();
}

PushMessagingServiceFactory::PushMessagingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PushMessagingProfileService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

PushMessagingServiceFactory::~PushMessagingServiceFactory() = default;

KeyedService* PushMessagingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PushMessagingServiceImpl(context);
}

}  // namespace neva_app_runtime
