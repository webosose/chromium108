// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {
class UsersPrivateDelegate;

// BrowserContextKeyedServiceFactory for each UsersPrivateDelegate.
class UsersPrivateDelegateFactory : public ProfileKeyedServiceFactory {
 public:
  static UsersPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static UsersPrivateDelegateFactory* GetInstance();

  UsersPrivateDelegateFactory(const UsersPrivateDelegateFactory&) = delete;
  UsersPrivateDelegateFactory& operator=(const UsersPrivateDelegateFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<UsersPrivateDelegateFactory>;

  UsersPrivateDelegateFactory();
  ~UsersPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_FACTORY_H__
