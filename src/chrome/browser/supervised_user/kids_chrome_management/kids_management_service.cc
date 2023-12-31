// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_management_service.h"

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace {
using ::content::BrowserContext;
}  // namespace

// Builds the service instance and its local dependencies.
// The profile dependency is needed to verify the dynamic child account status.
KeyedService* KidsManagementServiceFactory::BuildServiceInstanceFor(
    BrowserContext* browser_context) const {
  return new KidsManagementService();
}
