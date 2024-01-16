// Copyright 2024 LG Electronics, Inc.
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

#ifndef NEVA_PAL_SERVICE_PUBLIC_PROXY_SETTING_DELEGATE_H_
#define NEVA_PAL_SERVICE_PUBLIC_PROXY_SETTING_DELEGATE_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/neva/proxy_settings.h"

namespace content {
class ContentBrowserClient;
}

namespace pal {

class ProxySettingDelegate
    : public base::RefCountedThreadSafe<ProxySettingDelegate> {
 public:
  virtual ~ProxySettingDelegate() {}

  virtual void ObserveSystemProxySetting(
      content::ContentBrowserClient* content_browser_client) = 0;
  virtual const content::ProxySettings GetProxySetting() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ProxySettingDelegate>;
};

}  // namespace pal

#endif  // NEVA_PAL_SERVICE_PUBLIC_PROXY_SETTING_DELEGATE_H_
