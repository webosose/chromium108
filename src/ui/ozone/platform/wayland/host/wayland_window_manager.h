// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandWindow;
class WaylandSubsurface;
class WaylandConnection;

// Stores and returns WaylandWindows. Clients that are interested in knowing
// when a new window is added or removed, but set self as an observer.
class WaylandWindowManager {
 public:
  explicit WaylandWindowManager(WaylandConnection* connection);
  WaylandWindowManager(const WaylandWindowManager&) = delete;
  WaylandWindowManager& operator=(const WaylandWindowManager&) = delete;
  ~WaylandWindowManager();

  void AddObserver(WaylandWindowObserver* observer);
  void RemoveObserver(WaylandWindowObserver* observer);

  // Notifies observers that the Window has been ack configured and
  // WaylandBufferManagerHost can start attaching buffers to the |surface_|.
  void NotifyWindowConfigured(WaylandWindow* window);

  // Notifies observers that the window's wayland role has been assigned.
  void NotifyWindowRoleAssigned(WaylandWindow* window);

  // Stores the window that should grab the located events.
  void GrabLocatedEvents(WaylandWindow* event_grabber);

  // Removes the window that should grab the located events.
  void UngrabLocatedEvents(WaylandWindow* event_grabber);

  // Returns current event grabber.
  WaylandWindow* located_events_grabber() const {
    return located_events_grabber_;
  }

#if defined(OS_WEBOS)
  // Stores the |device_id| and |grabber| that should grab the keyboard events.
  void GrabKeyboardEvents(int device_id, WaylandWindow* grabber);

  // Removes the |device_id| and |grabber| that should grab the keyboard events.
  void UngrabKeyboardEvents(int device_id, WaylandWindow* grabber);

  // Returns current keyboard events grabber by |device_id|.
  WaylandWindow* keyboard_events_grabber(int device_id) const;

  // Stores the |device_id| and |grabber| that should grab the touch events.
  void GrabTouchEvents(int device_id, WaylandWindow* grabber);

  // Removes the |device_id| and |grabber| that should grab the touch events.
  void UngrabTouchEvents(int device_id, WaylandWindow* grabber);

  // Returns current touch events grabber by |device_id|.
  WaylandWindow* touch_events_grabber(int device_id) const;

  // Stores the |device_id| and |grabber| that should grab the pointer events.
  void GrabPointerEvents(int device_id, WaylandWindow* grabber);

  // Removes the |device_id| and |grabber| that should grab the pointer events.
  void UngrabPointerEvents(int device_id, WaylandWindow* grabber);

  // Returns current pointer events grabber by |device_id|.
  WaylandWindow* pointer_events_grabber(int device_id) const;
#endif  // defined(OS_WEBOS)

  // Returns a window found by |widget|.
  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget) const;

  // Returns a window with largests bounds.
  WaylandWindow* GetWindowWithLargestBounds() const;

  // Returns a current active window.
  WaylandWindow* GetCurrentActiveWindow() const;

  // Returns a current focused window by pointer, touch, or keyboard.
  WaylandWindow* GetCurrentFocusedWindow() const;

  // Returns a current focused window by pointer or touch.
  WaylandWindow* GetCurrentPointerOrTouchFocusedWindow() const;

  // Returns a current focused window by pointer.
  WaylandWindow* GetCurrentPointerFocusedWindow() const;

  // Returns a current focused window by touch.
  WaylandWindow* GetCurrentTouchFocusedWindow() const;

  // Returns a current focused window by keyboard.
  WaylandWindow* GetCurrentKeyboardFocusedWindow() const;

  // Sets the given window as the pointer focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means pointer focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetPointerFocusedWindow(WaylandWindow* window);

  // Sets the given window as the touch focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means touch focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetTouchFocusedWindow(WaylandWindow* window);

  // Sets the given window as the keyboard focused window.
  // If there already is another, the old one will be unset.
  // If nullptr is passed to |window|, it means keyboard focus is unset from
  // any window.
  // The given |window| must be managed by this manager.
  void SetKeyboardFocusedWindow(WaylandWindow* window);

  // Returns all stored windows.
  std::vector<WaylandWindow*> GetAllWindows() const;

  // Returns true if the |window| still exists.
  bool IsWindowValid(const WaylandWindow* window) const;

  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);
  void AddSubsurface(gfx::AcceleratedWidget widget,
                     WaylandSubsurface* subsurface);
  void RemoveSubsurface(gfx::AcceleratedWidget widget,
                        WaylandSubsurface* subsurface);

  // Creates a new unique gfx::AcceleratedWidget.
  gfx::AcceleratedWidget AllocateAcceleratedWidget();

 private:
  WaylandWindow* pointer_focused_window_ = nullptr;
  WaylandWindow* keyboard_focused_window_ = nullptr;

  const raw_ptr<WaylandConnection> connection_;

  base::ObserverList<WaylandWindowObserver> observers_;

  base::flat_map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  raw_ptr<WaylandWindow> located_events_grabber_ = nullptr;

#if defined(OS_WEBOS)
  // Stores keyboard device (e.g., virtual keyboard) identifiers and related
  // events grabbers (windows).
  base::flat_map<int, WaylandWindow*> keyboard_events_grabber_map_;

  // Stores touch device (e.g., touchscreen) identifiers and related events
  // grabbers (windows).
  base::flat_map<int, WaylandWindow*> touch_events_grabber_map_;

  // Stores pointer device (e.g., mouse HID) identifiers and related
  // events grabbers (windows).
  base::flat_map<int, WaylandWindow*> pointer_events_grabber_map_;
#endif  // defined(OS_WEBOS)

  // Stores strictly monotonically increasing counter for allocating unique
  // AccelerateWidgets.
  gfx::AcceleratedWidget last_accelerated_widget_ = gfx::kNullAcceleratedWidget;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_
