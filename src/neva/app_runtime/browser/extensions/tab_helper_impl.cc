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

#include "neva/app_runtime/browser/extensions/tab_helper_impl.h"

#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_page_view.h"
#include "neva/app_runtime/app/app_runtime_shell_environment.h"

namespace neva {

content::WebContents* TabHelperImpl::GetWebContentsFromId(uint64_t id) {
  neva_app_runtime::PageContents* page_contents =
      neva_app_runtime::ShellEnvironment::GetInstance()->GetContentsPtr(id);

  if (page_contents)
    return page_contents->GetWebContents();
  return nullptr;
}

uint64_t TabHelperImpl::GetIdFromWebContents(
    content::WebContents* web_contents) {
  return neva_app_runtime::ShellEnvironment::GetInstance()->GetID(web_contents);
}

views::View* TabHelperImpl::GetViewFromId(uint64_t view_id) {
  neva_app_runtime::PageView* page_view =
      neva_app_runtime::ShellEnvironment::GetInstance()->GetViewPtr(view_id);

  if (page_view)
    return page_view->GetView();
  return nullptr;
}

}  // namespace neva
