// Copyright 2022 LG Electronics, Inc.
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

#ifndef NEVA_EXTENSIONS_BROWSER_API_TABS_TABS_API_H_
#define NEVA_EXTENSIONS_BROWSER_API_TABS_TABS_API_H_

#include "extensions/browser/extension_function.h"

namespace neva {

// Tabs
class TabsCreateFunction : public ExtensionFunction {
  ~TabsCreateFunction() override {}
  ResponseAction Run() override;
  void OnTabCreated(int tab_id);
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};

}  // namespace neva

#endif  // NEVA_EXTENSIONS_BROWSER_API_TABS_TABS_API_H_
