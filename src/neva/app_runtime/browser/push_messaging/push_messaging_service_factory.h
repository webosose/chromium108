// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/push_messaging/push_messaging_service_factory.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace neva_app_runtime {
class PushMessagingServiceImpl;

class PushMessagingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PushMessagingServiceImpl* GetForProfile(
      content::BrowserContext* profile);
  static PushMessagingServiceFactory* GetInstance();

  PushMessagingServiceFactory(const PushMessagingServiceFactory&) = delete;
  PushMessagingServiceFactory& operator=(const PushMessagingServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<PushMessagingServiceFactory>;

  PushMessagingServiceFactory();
  ~PushMessagingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace neva_app_runtime

#endif  //  NEVA_APP_RUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
