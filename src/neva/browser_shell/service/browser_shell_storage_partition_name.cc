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

#include "base/strings/string_util.h"

namespace browser_shell {

void ParseStoragePartitionName(const std::string& spec,
                               std::string& name,
                               bool& off_the_record) {
  if (spec.empty()) {
    name = "";
    off_the_record = false;
  } else if (base::StartsWith(spec, "persist", base::CompareCase::SENSITIVE)) {
    name = spec.substr(8);
    off_the_record = false;
  } else if (base::StartsWith(spec, "guest", base::CompareCase::SENSITIVE)) {
    // That's not real guest WebContents. Handling 'guest' prefix
    // this way is made to be compatible with Neva Browser.
    name = spec.substr(6);
    off_the_record = true;
  } else {
    name = spec;
    off_the_record = true;
  }
}

}  // namespace neva_app_runtime
