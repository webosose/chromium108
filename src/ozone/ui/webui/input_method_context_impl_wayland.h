// Copyright 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_WEBUI_INPUT_METHOD_CONTEXT_IMPL_WAYLAND_H_
#define OZONE_UI_WEBUI_INPUT_METHOD_CONTEXT_IMPL_WAYLAND_H_

#include <string>

#include "ozone/platform/ozone_export_wayland.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class InputMethodContextManager;

// An implementation of LinuxInputMethodContext for IME support on Ozone
// platform using Wayland.
class OZONE_WAYLAND_EXPORT InputMethodContextImplWayland
    : public LinuxInputMethodContext {
 public:
  InputMethodContextImplWayland(
      ui::LinuxInputMethodContextDelegate* delegate,
      unsigned handle,
      ui::InputMethodContextManager* input_context_manager);
  InputMethodContextImplWayland(const InputMethodContextImplWayland&) = delete;
  InputMethodContextImplWayland& operator=(
      const InputMethodContextImplWayland&) = delete;
  ~InputMethodContextImplWayland() override;

  // overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  bool IsPeekKeyEvent(const ui::KeyEvent& key_event) override;
  void Reset() override;
  void UpdateFocus(bool has_client,
                   TextInputType old_type,
                   TextInputType new_type) override;
  void SetContentType(TextInputType type,
                      TextInputMode mode,
                      uint32_t flags,
                      bool should_do_learning) override;
  void SetGrammarFragmentAtCursor(const ui::GrammarFragment& fragment) override;
  void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                          const gfx::Rect& autocorrect_bounds) override;
  void SetCursorLocation(const gfx::Rect&) override;
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& selection_range) override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;

  unsigned GetHandle() const { return handle_; }

  void Commit(const std::string& text);
  void PreeditChanged(const std::string& text,
                      const std::string& commit);
  void DeleteRange(int32_t index, uint32_t length);

 private:
  // Must not be NULL.
  LinuxInputMethodContextDelegate* delegate_;
  InputMethodContextManager* input_method_context_manager_;
  unsigned handle_;
};

}  // namespace ui

#endif  //  OZONE_UI_WEBUI_INPUT_METHOD_CONTEXT_IMPL_WAYLAND_H_
