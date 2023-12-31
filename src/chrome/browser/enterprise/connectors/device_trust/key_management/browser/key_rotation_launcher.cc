// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher_impl.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
// #include
// "components/enterprise/browser/controller/browser_dm_token_storage.h"
// #include "components/policy/core/common/cloud/device_management_service.h"

namespace enterprise_connectors {

// static
std::unique_ptr<KeyRotationLauncher> KeyRotationLauncher::Create(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_prefs) {
  return std::make_unique<KeyRotationLauncherImpl>(
      dm_token_storage, device_management_service,
      std::move(url_loader_factory), local_prefs);
}

}  // namespace enterprise_connectors
