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

#include "neva/extensions/browser/api/extension_action/extension_action_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"

namespace neva {

ExtensionActionAPI::ExtensionActionAPI(content::BrowserContext* context)
    : browser_context_(context) {}

ExtensionActionAPI::~ExtensionActionAPI() {}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    ExtensionActionAPI>>::DestructorAtExit g_extension_action_api_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
extensions::BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
ExtensionActionAPI::GetFactoryInstance() {
  return g_extension_action_api_factory.Pointer();
}

// static
ExtensionActionAPI* ExtensionActionAPI::Get(content::BrowserContext* context) {
  return extensions::BrowserContextKeyedAPIFactory<ExtensionActionAPI>::Get(
      context);
}

void ExtensionActionAPI::DispatchExtensionActionClicked(
    const std::string& extension_id,
    const uint64_t tab_id,
    content::WebContents* web_contents) {
  // TODO(neva): In chrome browser, there are much more arguments such as
  // favIconUrl, width/height, index, openerTabId, title, windowId.
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  base::Value::Dict dict;
  dict.Set("url", web_contents->GetLastCommittedURL().spec());
  dict.Set("id", static_cast<int>(tab_id));
  args->Append(std::move(dict));

  extensions::events::HistogramValue histogram_value =
      extensions::events::UNKNOWN;
  auto event = std::make_unique<extensions::Event>(
      histogram_value, "action.onClicked", std::move(args->GetList()),
      browser_context_);
  event->user_gesture = extensions::EventRouter::USER_GESTURE_ENABLED;
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ExtensionActionAPI::Shutdown() {}

//
// ExtensionActionFunction
//

ExtensionActionFunction::ExtensionActionFunction() {}

ExtensionActionFunction::~ExtensionActionFunction() {}

ExtensionFunction::ResponseAction ExtensionActionFunction::Run() {
  return RespondNow(NoArguments());
}

}  // namespace neva
