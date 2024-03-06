// Copyright 2023 LG Electronics, Inc.
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

#include "neva/injection/public/renderer/installable_manager_webapi.h"
#include "neva/injection/renderer/installable_manager/installable_manager_injection.h"

namespace injections {

// static
void InstallableManagerWebAPI::Install(blink::WebLocalFrame* frame,
                                       const std::string&) {
  InstallableManagerInjection::Install(frame);
}

// static
void InstallableManagerWebAPI::Uninstall(blink::WebLocalFrame* frame) {
  InstallableManagerInjection::Uninstall(frame);
}

}  // namespace injections
