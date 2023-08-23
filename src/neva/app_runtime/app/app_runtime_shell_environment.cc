// Copyright 2021 LG Electronics, Inc.
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

#include "neva/app_runtime/app/app_runtime_shell_environment.h"

#include <tuple>

#include "neva/app_runtime/app/app_runtime_page_contents.h"
#include "neva/app_runtime/app/app_runtime_page_view.h"

namespace neva_app_runtime {

ShellEnvironment* ShellEnvironment::GetInstance() {
  return base::Singleton<ShellEnvironment>::get();
}

ShellEnvironment::ShellEnvironment() = default;
ShellEnvironment::~ShellEnvironment() = default;

uint64_t ShellEnvironment::GetNextID() {
  return ++id_counter_;
}

uint64_t ShellEnvironment::GetNextIDFor(PageView* ptr) {
  const uint64_t id = ++id_counter_;
  id_to_view_ptr_[id] = ptr;
  view_ptr_to_id_[ptr] = id;
  return id;
}

uint64_t ShellEnvironment::GetNextIDFor(PageContents* ptr) {
  const uint64_t id = ++id_counter_;
  id_to_contents_ptr_[id] = ptr;
  contents_ptr_to_id_[ptr] = id;
  return id;
}

uint64_t ShellEnvironment::GetID(PageView* ptr) const {
  auto it = view_ptr_to_id_.find(ptr);
  if (it == view_ptr_to_id_.cend())
    return 0;
  return it->second;
}

uint64_t ShellEnvironment::GetID(PageContents* ptr) const {
  auto it = contents_ptr_to_id_.find(ptr);
  if (it == contents_ptr_to_id_.cend())
    return 0;
  return it->second;
}

uint64_t ShellEnvironment::GetID(content::WebContents* ptr) const {
  for (const auto& item : contents_ptr_to_id_) {
    if (!item.first->GetWebContents())
      continue;

    if (item.first->GetWebContents() == ptr)
      return item.second;
  }
  return 0;
}

PageView* ShellEnvironment::GetViewPtr(uint64_t id) const {
  auto it = id_to_view_ptr_.find(id);
  if (it == id_to_view_ptr_.cend())
    return nullptr;
  return it->second;
}

PageContents* ShellEnvironment::GetContentsPtr(uint64_t id) const {
  auto it = id_to_contents_ptr_.find(id);
  if (it == id_to_contents_ptr_.cend())
    return nullptr;
  return it->second;
}

void ShellEnvironment::Remove(PageView* ptr) {
  auto it = view_ptr_to_id_.find(ptr);
  if (it != view_ptr_to_id_.end()) {
    std::ignore = id_to_view_ptr_.erase(it->second);
    std::ignore = view_ptr_to_id_.erase(it);
  }

  PageViewPtr released_view;
  auto detached_it = detached_views_.find(ptr);
  if (detached_it != detached_views_.end()) {
    // detached_views_.erase(ptr) may cause recursive deletion
    // items in detached_views_, so empty unique_ptr terminate
    // that recursion in any case.
    released_view = std::move(detached_it->second);
    std::ignore = detached_views_.erase(ptr);
  }
}

void ShellEnvironment::Remove(PageContents* ptr) {
  auto it = contents_ptr_to_id_.find(ptr);
  if (it != contents_ptr_to_id_.end()) {
    std::ignore = id_to_contents_ptr_.erase(it->second);
    std::ignore = contents_ptr_to_id_.erase(it);
  }

  PageContentsPtr released_content;
  auto detached_it = detached_contents_.find(ptr);
  if (detached_it != detached_contents_.end()) {
    // detached_contents_.erase(ptr) may cause recursive deletion
    // items in detached_contents_, so empty unique_ptr terminate
    // that recursion in any case.
    released_content = std::move(detached_it->second);
    std::ignore = detached_contents_.erase(ptr);
  }
}

void ShellEnvironment::RemoveView(uint64_t id) {
  auto it = id_to_view_ptr_.find(id);
  if (it != id_to_view_ptr_.end())
    Remove(it->second);
}

void ShellEnvironment::RemoveContents(uint64_t id) {
  auto it = id_to_contents_ptr_.find(id);
  if (it != id_to_contents_ptr_.end())
    Remove(it->second);
}

ShellEnvironment::PageViewPtr ShellEnvironment::SaveDetachedView(
    PageViewPtr page_view) {
  detached_views_[page_view.get()] = std::move(page_view);
  return PageViewPtr();
}

ShellEnvironment::PageContentsPtr ShellEnvironment::SaveDetachedContents(
    PageContentsPtr page_contents) {
  detached_contents_[page_contents.get()] = std::move(page_contents);
  return PageContentsPtr();
}

ShellEnvironment::PageViewPtr ShellEnvironment::ReleaseDetachedView(
    PageView* ptr) {
  auto it = detached_views_.find(ptr);
  if (it != detached_views_.end()) {
    PageViewPtr ret(std::move(it->second));
    detached_views_.erase(it);
    return ret;
  }
  return PageViewPtr();
}

ShellEnvironment::PageContentsPtr ShellEnvironment::ReleaseDetachedContents(
    PageContents* ptr) {
  auto it = detached_contents_.find(ptr);
  if (it != detached_contents_.end()) {
    PageContentsPtr ret(std::move(it->second));
    detached_contents_.erase(it);
    return ret;
  }
  return PageContentsPtr();
}

}  // namespace neva_app_runtime
