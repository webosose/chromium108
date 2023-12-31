// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"

#include "ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace multidevice_setup {

// static
OobeCompletionTracker* OobeCompletionTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OobeCompletionTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OobeCompletionTrackerFactory* OobeCompletionTrackerFactory::GetInstance() {
  return base::Singleton<OobeCompletionTrackerFactory>::get();
}

OobeCompletionTrackerFactory::OobeCompletionTrackerFactory()
    : ProfileKeyedServiceFactory("OobeCompletionTrackerFactory") {}

OobeCompletionTrackerFactory::~OobeCompletionTrackerFactory() = default;

KeyedService* OobeCompletionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OobeCompletionTracker();
}

}  // namespace multidevice_setup
}  // namespace ash
