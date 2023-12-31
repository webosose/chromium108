// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos::network_health {
class NetworkHealthService;
}

namespace ash {

namespace network_diagnostics {
class NetworkDiagnostics;
}

namespace network_health {

class NetworkHealthManager {
 public:
  static NetworkHealthManager* GetInstance();

  NetworkHealthManager();
  ~NetworkHealthManager() = delete;

  mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
  GetHealthRemoteAndBindReceiver();
  mojo::PendingRemote<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
  GetDiagnosticsRemoteAndBindReceiver();

  void BindHealthReceiver(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);
  void BindDiagnosticsReceiver(
      mojo::PendingReceiver<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          receiver);

  void AddObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver> observer);

 private:
  std::unique_ptr<chromeos::network_health::NetworkHealthService>
      network_health_service_;
  std::unique_ptr<network_diagnostics::NetworkDiagnostics> network_diagnostics_;
};

}  // namespace network_health
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_MANAGER_H_
