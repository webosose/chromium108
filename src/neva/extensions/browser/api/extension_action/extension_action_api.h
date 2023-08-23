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

#ifndef NEVA_EXTENSIONS_BROWSER_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
#define NEVA_EXTENSIONS_BROWSER_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace neva {

class ExtensionActionAPI : public extensions::BrowserContextKeyedAPI {
 public:
  explicit ExtensionActionAPI(content::BrowserContext* context);

  ExtensionActionAPI(const ExtensionActionAPI&) = delete;
  ExtensionActionAPI& operator=(const ExtensionActionAPI&) = delete;

  ~ExtensionActionAPI() override;

  static ExtensionActionAPI* Get(content::BrowserContext* context);

  static extensions::BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
  GetFactoryInstance();

  // Dispatches the onClicked event for extension that owns the given action.
  void DispatchExtensionActionClicked(const std::string& extension_id,
                                      const uint64_t tab_id,
                                      content::WebContents* web_contents);

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<ExtensionActionAPI>;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static const char* service_name() { return "ExtensionActionAPI"; }

  raw_ptr<content::BrowserContext> browser_context_;
};

class ExtensionActionFunction : public ExtensionFunction {
 protected:
  ExtensionActionFunction();
  ~ExtensionActionFunction() override;
  ResponseAction Run() override;
};

}  // namespace neva

#endif  // NEVA_EXTENSIONS_BROWSER_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
