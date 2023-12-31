// Copyright 2020 LG Electronics, Inc.
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

#ifndef NEVA_INJECTION_RENDERER_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_DATAMANAGER_H_
#define NEVA_INJECTION_RENDERER_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_DATAMANAGER_H_

#include <string>

#include "neva/injection/renderer/injection_data_manager.h"

namespace injections {

class WebOSGAVDataManager : public InjectionDataManager {
 public:
  explicit WebOSGAVDataManager(const std::string& json);
  ~WebOSGAVDataManager() override = default;

  bool GetInitialisedStatus() const { return initialized_; }

  void SetInitializedStatus(bool status) { initialized_ = status; }

  void DoInitialize(const std::string& json);

 private:
  bool initialized_ = false;
};

}  // namespace injections

#endif  // NEVA_INJECTION_RENDERER_WEBOSGAVPLUGIN_WEBOSGAVPLUGIN_DATAMANAGER_H_
