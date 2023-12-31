// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_
#define REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_

#include <memory>

#include <xkbcommon/xkbcommon.h>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/wayland_connection.h"
#include "remoting/host/linux/wayland_display.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Helper class that facilitates interaction of different Wayland related
// components under chromoting.
class WaylandManager {
 public:
  using DesktopMetadataCallbackSignature = void(webrtc::DesktopCaptureMetadata);
  using DesktopMetadataCallback =
      base::RepeatingCallback<DesktopMetadataCallbackSignature>;
  using UpdateScreenResolutionSignature = void(ScreenResolution,
                                               webrtc::ScreenId);
  using UpdateScreenResolutionCallback =
      base::RepeatingCallback<UpdateScreenResolutionSignature>;
  using KeyboardLayoutCallback =
      base::RepeatingCallback<void(XkbKeyMapUniquePtr)>;
  using KeyboardModifiersCallbackSignature = void(uint32_t group);
  using KeyboardModifiersCallback =
      base::RepeatingCallback<KeyboardModifiersCallbackSignature>;

  WaylandManager();
  ~WaylandManager();
  WaylandManager(const WaylandManager&) = delete;
  WaylandManager& operator=(const WaylandManager&) = delete;

  static WaylandManager* Get();

  // The singleton instance should be initialized by the host process on the
  // UI thread right after creation.
  void Init(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Adds callback to be invoked when a desktop capturer has metadata available.
  void AddCapturerMetadataCallback(DesktopMetadataCallback callback);

  // Invoked by the desktop capturer(s), upon successful start.
  void OnDesktopCapturerMetadata(webrtc::DesktopCaptureMetadata metadata);

  // Adds callback to be invoked when screen resolution is updated by the
  // desktop resizer.
  void AddUpdateScreenResolutionCallback(
      UpdateScreenResolutionCallback callback);

  // Invoked by the desktop_resizer_wayland upon screen resolution update from
  // resizing_host_observer.
  void OnUpdateScreenResolution(ScreenResolution resolution,
                                webrtc::ScreenId screen_id);

  // Sets callback to be invoked when new keyboard layout is detected.
  void SetKeyboardLayoutCallback(KeyboardLayoutCallback callback);

  // Invoked by the wayland keyboard, upon detecting a new keyboard layout
  // mapping from the compositor.
  void OnKeyboardLayout(XkbKeyMapUniquePtr);

  // Adds callback to be invoked when new keyboard layout is detected.
  void AddKeyboardModifiersCallback(KeyboardModifiersCallback callback);

  // Invoked by the wayland keyboard, upon detecting a keyboard modifier
  // changes from the compositor.
  void OnKeyboardModifiers(uint32_t group);

  // Gets the current information about displays available on the host.
  DesktopDisplayInfo GetCurrentDisplayInfo();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::unique_ptr<WaylandConnection> wayland_connection_;
  base::RepeatingCallbackList<DesktopMetadataCallbackSignature>
      capturer_metadata_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingCallbackList<UpdateScreenResolutionSignature>
      screen_resolution_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  KeyboardLayoutCallback keyboard_layout_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingCallbackList<KeyboardModifiersCallbackSignature>
      keyboard_modifier_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_
