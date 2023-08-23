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

#ifndef NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_ENVIRONMENT_H_
#define NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_ENVIRONMENT_H_

#include <stdint.h>
#include <map>
#include <memory>

#include "base/memory/singleton.h"

namespace content {
class WebContents;
}

namespace neva_app_runtime {

class PageContents;
class PageView;

class ShellEnvironment {
 public:
  using PageViewPtr = std::unique_ptr<PageView>;
  using PageContentsPtr = std::unique_ptr<PageContents>;

  static ShellEnvironment* GetInstance();

  ShellEnvironment(const ShellEnvironment&) = delete;
  ShellEnvironment& operator=(const ShellEnvironment&) = delete;

  uint64_t GetNextID();
  uint64_t GetNextIDFor(PageView* ptr);
  uint64_t GetNextIDFor(PageContents* ptr);

  uint64_t GetID(PageView* ptr) const;
  uint64_t GetID(PageContents* ptr) const;
  uint64_t GetID(content::WebContents* ptr) const;
  PageView* GetViewPtr(uint64_t id) const;
  PageContents* GetContentsPtr(uint64_t id) const;

  void Remove(PageView* ptr);
  void Remove(PageContents* ptr);
  void RemoveView(uint64_t id);
  void RemoveContents(uint64_t id);

  PageViewPtr SaveDetachedView(PageViewPtr page_view);
  PageContentsPtr SaveDetachedContents(PageContentsPtr page_contents);

  PageViewPtr ReleaseDetachedView(PageView* ptr);
  PageContentsPtr ReleaseDetachedContents(PageContents* ptr);

 private:
  friend struct base::DefaultSingletonTraits<ShellEnvironment>;
  ShellEnvironment();
  ~ShellEnvironment();

  uint64_t id_counter_ = 0;
  std::map<uint64_t, PageView*> id_to_view_ptr_;
  std::map<uint64_t, PageContents*> id_to_contents_ptr_;
  std::map<PageView*, uint64_t> view_ptr_to_id_;
  std::map<PageContents*, uint64_t> contents_ptr_to_id_;

  std::map<PageView*, PageViewPtr> detached_views_;
  std::map<PageContents*, PageContentsPtr> detached_contents_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_ENVIRONMENT_H_
