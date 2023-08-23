// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on chrome/browser/gcm/gcm_profile_service_factory.h
// android and offline page related code are removed.

#include "neva/app_runtime/browser/gcm/gcm_profile_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace gcm {

namespace {

// Requests a ProxyResolvingSocketFactory on the UI thread. Note that a WeakPtr
// of GCMProfileService is needed to detect when the KeyedService shuts down,
// and avoid calling into |profile| which might have also been destroyed.
void RequestProxyResolvingSocketFactoryOnUIThread(
    content::BrowserContext* profile,
    base::WeakPtr<GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service)
    return;
  network::mojom::NetworkContext* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

// A thread-safe wrapper to request a ProxyResolvingSocketFactory.
void RequestProxyResolvingSocketFactory(
    content::BrowserContext* profile,
    base::WeakPtr<GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread, profile,
                     std::move(service), std::move(receiver)));
}

}  // namespace

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // GCM is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return NULL;

  return static_cast<GCMProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GCMProfileServiceFactory* GCMProfileServiceFactory::GetInstance() {
  static base::NoDestructor<GCMProfileServiceFactory> instance;
  return instance.get();
}

GCMProfileServiceFactory::GCMProfileServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "GCMProfileService",
          BrowserContextDependencyManager::GetInstance()) {}

GCMProfileServiceFactory::~GCMProfileServiceFactory() = default;

KeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  std::unique_ptr<GCMProfileService> service;

  PrefService* prefs = user_prefs::UserPrefs::Get(context);

  service = std::make_unique<GCMProfileService>(
      prefs, context->GetPath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, context),
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      content::GetNetworkConnectionTracker(), version_info::Channel::UNKNOWN,
      "org.chromium.linux", nullptr, std::make_unique<GCMClientFactory>(),
      content::GetUIThreadTaskRunner({}), content::GetIOThreadTaskRunner({}),
      blocking_task_runner);

  return service.release();
}

}  // namespace gcm
