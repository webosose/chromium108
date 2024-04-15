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

#include "services/network/public/cpp/neva/cors_corb_exception.h"

#include <set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace {
std::set<int>& GetExceptionProcesses() {
  static base::NoDestructor<std::set<int>> processes;
  return *processes;
}

std::set<GURL>& GetExceptionUrlsSet() {
  static base::NoDestructor<std::set<GURL>> urls;
  return *urls;
}

}  // namespace

namespace network {
namespace neva {

// static
void CorsCorbException::AddForProcess(int process_id) {
  std::set<int>& processes = GetExceptionProcesses();
  processes.insert(process_id);
}

// static
void CorsCorbException::RemoveForProcess(int process_id) {
  std::set<int>& processes = GetExceptionProcesses();
  processes.erase(process_id);
}

// static
bool CorsCorbException::ShouldAllowExceptionForProcess(int process_id) {
  std::set<int>& processes = GetExceptionProcesses();
  return base::Contains(processes, process_id);
}

// static
bool CorsCorbException::ApplyException(const CorsErrorStatus& error_status) {
  switch (error_status.cors_error) {
    case mojom::CorsError::kWildcardOriginNotAllowed:
    case mojom::CorsError::kMissingAllowOriginHeader:
    case mojom::CorsError::kHeaderDisallowedByPreflightResponse:
      VLOG(1) << "Allow cors for CORS error=" << error_status.cors_error;
      return true;
      break;
    default:
      break;
  }
  return false;
}

// static
void CorsCorbException::AddForURL(const GURL& url) {
  std::set<GURL>& urls = GetExceptionUrlsSet();
  urls.insert(url);
}

// static
void CorsCorbException::RemoveForURL(const GURL& url) {
  std::set<GURL>& urls = GetExceptionUrlsSet();
  size_t number_of_elements_removed = urls.erase(url);
  DCHECK_EQ(1u, number_of_elements_removed);
}

// static
bool CorsCorbException::ShouldAllowExceptionForURL(const GURL& url) {
  std::set<GURL>& urls = GetExceptionUrlsSet();
  return base::Contains(urls, url);
}

}  // namespace neva
}  // namespace network
