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

#include "neva/extensions/browser/api/tabs/tabs_api.h"

#include "neva/extensions/browser/neva_extensions_service_factory.h"
#include "neva/extensions/browser/neva_extensions_service_impl.h"
#include "neva/extensions/common/api/tabs.h"

namespace neva {

namespace tabs = extensions::api::tabs;

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::unique_ptr<tabs::Create::Params> params(
      tabs::Create::Params::Create(args()));
  if (!params->create_properties.url)
    return RespondNow(Error("We don't support missing url yet."));

  NevaExtensionsServiceFactory::GetService(browser_context())
      ->OnExtensionTabCreationRequested(
          *(params->create_properties.url),
          base::BindOnce(&TabsCreateFunction::OnTabCreated, this));
  return RespondLater();
}

void TabsCreateFunction::OnTabCreated(int tab_id) {
  if (has_callback()) {
    tabs::Tab tab_object;
    tab_object.id = tab_id;
    Respond(WithArguments(tab_object.ToValue()));
    return;
  }
  Respond(NoArguments());
}

}  // namespace neva
