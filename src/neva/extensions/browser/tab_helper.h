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

#ifndef NEVA_EXTENSIONS_BROWSER_TAB_HELPER_H_
#define NEVA_EXTENSIONS_BROWSER_TAB_HELPER_H_

#include <stdint.h>

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace neva {

class TabHelper {
 public:
  TabHelper() = default;
  TabHelper(const TabHelper&) = delete;
  TabHelper& operator=(const TabHelper&) = delete;
  virtual ~TabHelper() = default;

  virtual content::WebContents* GetWebContentsFromId(uint64_t id) = 0;
  virtual uint64_t GetIdFromWebContents(content::WebContents* web_contents) = 0;
  virtual views::View* GetViewFromId(uint64_t view_id);
};

}  // namespace neva

#endif  // NEVA_EXTENSIONS_BROWSER_TAB_HELPER_H_
