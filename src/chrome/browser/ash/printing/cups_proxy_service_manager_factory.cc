// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/cups_proxy_service_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
CupsProxyServiceManagerFactory* CupsProxyServiceManagerFactory::GetInstance() {
  static base::NoDestructor<CupsProxyServiceManagerFactory> factory;
  return factory.get();
}

// static
CupsProxyServiceManager* CupsProxyServiceManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CupsProxyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CupsProxyServiceManagerFactory::CupsProxyServiceManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "CupsProxyServiceManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {}

CupsProxyServiceManagerFactory::~CupsProxyServiceManagerFactory() = default;

KeyedService* CupsProxyServiceManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // We do not need an instance of CupsProxyServiceManager on the lockscreen.
  if (!ProfileHelper::IsRegularProfile(Profile::FromBrowserContext(context))) {
    return nullptr;
  }
  return new CupsProxyServiceManager();
}

content::BrowserContext* CupsProxyServiceManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool CupsProxyServiceManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool CupsProxyServiceManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
