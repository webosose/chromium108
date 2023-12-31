// Copyright 2019 LG Electronics, Inc.
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

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_NEVA_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_NEVA_H_

#include <cstdint>
#include <memory>

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandCursor;
class WaylandKeyboard;
class WaylandPointer;
class WaylandTouch;
class WaylandCursorPosition;

namespace neva {

// Wayland seat wrapper implementation.
// TODO(neva): M99: Deprecate Neva WaylandSeat in favor of upstream one
class WaylandSeat {
 public:
  WaylandSeat(WaylandConnection* connection,
              std::uint32_t seat_id,
              wl_seat* seat);
  WaylandSeat(const WaylandSeat&) = delete;
  WaylandSeat& operator=(const WaylandSeat&) = delete;
  virtual ~WaylandSeat();

  std::uint32_t seat_id() const { return seat_id_; }
  wl_seat* seat() const { return seat_.get(); }
  WaylandCursor* cursor() const { return cursor_.get(); }
  WaylandCursorPosition* cursor_position() const {
    return cursor_position_.get();
  }
  WaylandKeyboard* keyboard() const { return keyboard_.get(); }
  WaylandPointer* pointer() const { return pointer_.get(); }
  WaylandTouch* touch() const { return touch_.get(); }

  // Creates WaylandKeyboard with the currently acquired protocol objects, if
  // possible. Returns true if WaylandKeyboard was created.
  bool CreateKeyboard();

 private:
  void UpdateInputDevices(wl_seat* seat, std::uint32_t capabilities);

  // wl_seat_listener
  static void Capabilities(void* data,
                           wl_seat* seat,
                           std::uint32_t capabilities);
  static void Name(void* data, wl_seat* seat, const char* name);

  WaylandConnection* const connection_;

  const std::uint32_t seat_id_;
  wl::Object<wl_seat> seat_;

  std::unique_ptr<WaylandCursor> cursor_;
  std::unique_ptr<WaylandCursorPosition> cursor_position_;
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandTouch> touch_;
};

}  // namespace neva
}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_NEVA_H_
