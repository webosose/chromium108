// Copyright 2016 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef WEBOS_PUBLIC_RUNTIME_H_
#define WEBOS_PUBLIC_RUNTIME_H_

#include <memory>
#include <string>

#include "neva/app_runtime/public/platform_factory.h"
#include "webos/common/webos_constants.h"
#include "webos/common/webos_export.h"

class LSHandle;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace webos {

class LunaServiceDelegate;
class CookieStoreUtilDelegate;
class PlatformDelegate;

class WEBOS_EXPORT Runtime {
 public:
  static Runtime* GetInstance();

  void InitializeLunaService(LunaServiceDelegate* luna_service_delegate);
  void InitializeCookieStoreUtil(
      CookieStoreUtilDelegate* cookie_store_util_delegate);
  void InitializePlatformDelegate(PlatformDelegate* platform_delegate) {
    platform_delegate_ = platform_delegate;
  }

  LSHandle* GetLSHandle();
  void FlushStoreCookie(PowerOffState power_off_state, std::string timestamp);

  void OnCursorVisibilityChanged(bool visible);

  bool GetMrcuStatus() { return is_mrcu_paired_; }
  void SetMrcuStatus(bool is_paired) { is_mrcu_paired_ = is_paired; }

  bool GetNetworkConnected() { return is_network_connected_; }
  void SetNetworkConnected(bool is_connected);

  bool GetIsForegroundAppEnyo() { return is_foreground_app_enyo_; }
  void SetIsForegroundAppEnyo(bool is_enyo) {
    is_foreground_app_enyo_ = is_enyo;
  }

  std::string GetBoardType() { return board_type_; }
  void SetBoardType(std::string board_type) { board_type_ = board_type; }

  std::string GetForegroundAppId() { return foreground_appid_; }
  void SetForegroundAppId(std::string appid) { foreground_appid_ = appid; }
  void SetLocale(std::string locale);

  void SetPlatformFactory(
      std::unique_ptr<neva_app_runtime::PlatformFactory> platform_factory);
  neva_app_runtime::PlatformFactory* get_platform_factory() {
    return platform_factory_.get();
  }

 private:
  friend struct base::DefaultSingletonTraits<Runtime>;

  // This object is is a singleton
  explicit Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  virtual ~Runtime();

  LunaServiceDelegate* luna_service_delegate_;
  CookieStoreUtilDelegate* cookie_store_util_delegate_;
  PlatformDelegate* platform_delegate_;

  bool is_mrcu_paired_;
  bool is_network_connected_;
  bool is_foreground_app_enyo_;

  std::string board_type_;
  std::string foreground_appid_;
  std::string current_locale_;

  std::unique_ptr<neva_app_runtime::PlatformFactory> platform_factory_;
};

}  // namespace webos

#endif  // WEBOS_PUBLIC_RUNTIME_H_
