// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
class StoragePartition;
class WebContents;
}  // namespace content

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace ash {
namespace login {

// Manages storage partitions for sign-in attempts on the sign-in screen and
// enrollment screen.
class SigninPartitionManager : public KeyedService {
 public:
  using ClearStoragePartitionTask =
      base::RepeatingCallback<void(content::StoragePartition* storage_partition,
                                   base::OnceClosure data_cleared)>;

  using GetSystemNetworkContextTask =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  using OnCreateNewStoragePartition =
      base::RepeatingCallback<void(content::StoragePartition*)>;

  using StartSigninSessionDoneCallback =
      base::OnceCallback<void(const std::string& partition_name)>;

  explicit SigninPartitionManager(content::BrowserContext* browser_context);

  SigninPartitionManager(const SigninPartitionManager&) = delete;
  SigninPartitionManager& operator=(const SigninPartitionManager&) = delete;

  ~SigninPartitionManager() override;

  // Creates a new StoragePartition for a sign-in attempt. If a previous
  // StoragePartition has been created by this SigninPartitionManager, it is
  // closed (and cleared).
  // `embedder_web_contents` is the WebContents instance embedding the webview
  // which will display the sign-in pages.
  // `signin_session_started` will be invoked with the partition name of the
  // started signin session on completion.
  void StartSigninSession(
      content::WebContents* embedder_web_contents,
      StartSigninSessionDoneCallback signin_session_started);

  // Closes the current StoragePartition. All cached data in the
  // StoragePartition is cleared. `partition_data_cleared` will be called when
  // clearing of cached data is finished.
  void CloseCurrentSigninSession(base::OnceClosure partition_data_cleared);

  // Returns true if a sign-in session is active, that is between
  // StartSigninSession and CloseCurrentSigninSession calls.
  bool IsInSigninSession() const;

  // Returns the current StoragePartition name. This can be used as a webview's
  // `partition` attribute. May only be called when a sign-in session is active,
  // that is between StartSigninSession and CloseCurrentSigninSession calls.
  const std::string& GetCurrentStoragePartitionName() const;

  // Returns the current StoragePartition. May only be called when a sign-in
  // session is active, that is between StartSigninSession and
  // CloseCurrentSigninSession calls.
  content::StoragePartition* GetCurrentStoragePartition();

  // Returns true if `storage_partition` is the partition in use by the current
  // sign-in session. Returns false if no sign-in session is active.
  bool IsCurrentSigninStoragePartition(
      const content::StoragePartition* storage_partition) const;

  // Disposes old storage partitions. Note that callers need to be sure that
  // old partitions will not be referenced again.
  void DisposeOldStoragePartitions();

  void SetClearStoragePartitionTaskForTesting(
      ClearStoragePartitionTask clear_storage_partition_task);
  void SetGetSystemNetworkContextForTesting(
      GetSystemNetworkContextTask get_system_network_context_task);
  void SetOnCreateNewStoragePartitionForTesting(
      OnCreateNewStoragePartition on_create_new_storage_partition);

  class Factory : public ProfileKeyedServiceFactory {
   public:
    static SigninPartitionManager* GetForBrowserContext(
        content::BrowserContext* browser_context);

    static Factory* GetInstance();

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
  };

 private:
  // Invoked after `storage_partition` is cleared.
  void OnStoragePartitionCleared(content::StoragePartition* storage_partition,
                                 base::OnceClosure partition_data_cleared);

  content::BrowserContext* const browser_context_;

  ClearStoragePartitionTask clear_storage_partition_task_;
  GetSystemNetworkContextTask get_system_network_context_task_;
  OnCreateNewStoragePartition on_create_new_storage_partition_;

  // GuestView StoragePartitions use the host of the embedder site's URL as the
  // domain of their StoragePartition.
  std::string storage_partition_domain_;
  // The random and unique name of the StoragePartition to be used, is generated
  // by SigninPartitionManager.
  std::string current_storage_partition_name_;
  // The StoragePartition identified by `storage_partition_domain_` and
  // `current_storage_partition_name_`.
  content::StoragePartition* current_storage_partition_ = nullptr;

  // Storage partitions that should be disposed. Storage partitions for previous
  // gaia loads are added here. It if only safe to remove them when there are no
  // outstanding references. The current assumption is that all references are
  // gone after the gaia loading webview is fully navigated away. The vector
  // here is maintained in the time order of gaia loads and the last element
  // is used as a barrier to indicate all previous ones could be safely
  // disposed.
  std::vector<content::StoragePartition*> pending_removal_partitions_;

  base::WeakPtrFactory<SigninPartitionManager> weak_ptr_factory_{this};
};

}  // namespace login
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace login {
using ::ash::login::SigninPartitionManager;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_PARTITION_MANAGER_H_
