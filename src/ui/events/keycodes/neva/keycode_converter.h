// Copyright 2020 LG Electronics, Inc.
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

#ifndef UI_EVENTS_KEYCODES_NEVA_KEYCODE_CONVERTER_H_
#define UI_EVENTS_KEYCODES_NEVA_KEYCODE_CONVERTER_H_

#include <stdint.h>

namespace ui {
namespace neva {

enum IMEModifierFlags : uint32_t { FLAG_SHFT = 1, FLAG_CTRL = 2, FLAG_ALT = 4 };

class KeycodeConverter {
 public:
  KeycodeConverter() = delete;

  static uint32_t KeyNumberFromKeySymCode(uint32_t key_sym, uint32_t modifiers);
  static int GetModifierKey(IMEModifierFlags key_sym);
  static bool IsKeyCodeNonPrintable(uint16_t keycode);
  static bool IsUnknown(uint32_t keycode);
};

}  // namespace neva
}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_NEVA_KEYCODE_CONVERTER_H_
