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

#include "ui/events/keycodes/neva/keycode_converter.h"

#include <linux/input.h>

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/xkb_keysym.h"

namespace ui {
namespace neva {

// static
// Convert keys from IME which are XKB keycodes or Qt keycodes to hardware
// keycodes
uint32_t KeycodeConverter::KeyNumberFromKeySymCode(uint32_t key_sym,
                                                   uint32_t modifiers) {
  switch (key_sym) {
    case XKB_KEY_Escape:
      return KEY_ESC;
    case XKB_KEY_F1:
      return KEY_F1;
    case XKB_KEY_F2:
      return KEY_F2;
    case XKB_KEY_F3:
      return KEY_F3;
    case XKB_KEY_F4:
      return KEY_F4;
    case XKB_KEY_F5:
      return KEY_F5;
    case XKB_KEY_F6:
      return KEY_F6;
    case XKB_KEY_F7:
      return KEY_F7;
    case XKB_KEY_F8:
      return KEY_F8;
    case XKB_KEY_F9:
      return KEY_F9;
    case XKB_KEY_F10:
      return KEY_F10;
    case XKB_KEY_F11:
      return KEY_F11;
    case XKB_KEY_F12:
      return KEY_F12;
    case XKB_KEY_BackSpace:
      return KEY_BACKSPACE;
    case XKB_KEY_Tab:
      return KEY_TAB;
    case XKB_KEY_Caps_Lock:
      return KEY_CAPSLOCK;
    case XKB_KEY_ISO_Enter:
    case XKB_KEY_Return:
      return KEY_ENTER;
    case XKB_KEY_Shift_L:
      return KEY_LEFTSHIFT;
    case XKB_KEY_Control_L:
      return KEY_LEFTCTRL;
    case XKB_KEY_Alt_L:
      return KEY_LEFTALT;
    case XKB_KEY_Scroll_Lock:
      return KEY_SCROLLLOCK;
    case XKB_KEY_Insert:
      return KEY_INSERT;
    case XKB_KEY_Delete:
      return KEY_DELETE;
    case XKB_KEY_Home:
      return KEY_HOME;
    case XKB_KEY_End:
      return KEY_END;
    case XKB_KEY_Prior:
      return KEY_PAGEUP;
    case XKB_KEY_Next:
      return KEY_PAGEDOWN;
    case XKB_KEY_Left:
      return KEY_LEFT;
    case XKB_KEY_Up:
      return KEY_UP;
    case XKB_KEY_Right:
      return KEY_RIGHT;
    case XKB_KEY_Down:
      return KEY_DOWN;
    case XKB_KEY_Num_Lock:
      return KEY_NUMLOCK;
    case XKB_KEY_KP_Enter:
      return KEY_KPENTER;
    case XKB_KEY_XF86Back:
      return KEY_PREVIOUS;
    case 0x2f:
      return KEY_KPSLASH;
    case 0x2d:
      return KEY_KPMINUS;
    case 0x2a:
      return KEY_KPASTERISK;
    case 0x37:
      return KEY_KP7;
    case 0x38:
      return KEY_KP8;
    case 0x39:
      return KEY_KP9;
    case 0x34:
      return KEY_KP4;
    case 0x35:
      return KEY_KP5;
    case 0x36:
      return KEY_KP6;
    case 0x31:
      return KEY_KP1;
    case 0x32:
      return KEY_KP2;
    case 0x33:
      return KEY_KP3;
    case 0x30:
      return KEY_KP0;
    case 0x2e:
      return KEY_KPDOT;
    case 0x2b:
      return KEY_KPPLUS;
    default:
      return KEY_UNKNOWN;
  }
}

// static
int KeycodeConverter::GetModifierKey(IMEModifierFlags key_sym) {
  return ui::EF_NONE;
}

// static
bool KeycodeConverter::IsKeyCodeNonPrintable(uint16_t keycode) {
  return false;
}

// static
bool KeycodeConverter::IsUnknown(uint32_t keycode) {
  return keycode == KEY_UNKNOWN;
}

}  // namespace neva
}  // namespace ui
