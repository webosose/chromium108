// Copyright 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/ui/webui/input_method_context_impl_wayland.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ozone/ui/webui/input_method_context_manager.h"
#include "ui/base/ime/composition_text.h"

constexpr SkColor kPreeditHighlightColor =
#if defined(OS_WEBOS)
    // specified by UX team
    SkColorSetARGB(0xFF, 198, 176, 186);
#else
    SK_ColorTRANSPARENT;
#endif

namespace ui {

InputMethodContextImplWayland::InputMethodContextImplWayland(
    LinuxInputMethodContextDelegate* delegate,
    unsigned handle,
    ui::InputMethodContextManager* input_context_manager)
    : delegate_(delegate),
      input_method_context_manager_(input_context_manager),
      handle_(handle) {
  CHECK(delegate_);
  input_method_context_manager_->AddContext(this);
}

InputMethodContextImplWayland::~InputMethodContextImplWayland() {
  input_method_context_manager_->RemoveContext(this);
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodContextImplWayland, ui::LinuxInputMethodContext implementation:
bool InputMethodContextImplWayland::DispatchKeyEvent(
    const KeyEvent& key_event) {
  return false;
}

bool InputMethodContextImplWayland::IsPeekKeyEvent(
    const ui::KeyEvent& key_event) {
  return false;
}

void InputMethodContextImplWayland::Reset() {
  input_method_context_manager_->ImeReset(handle_);
}

void InputMethodContextImplWayland::UpdateFocus(bool has_client,
                                                TextInputType old_type,
                                                TextInputType new_type) {}

void InputMethodContextImplWayland::SetContentType(TextInputType type,
                                                   TextInputMode mode,
                                                   uint32_t flags,
                                                   bool should_do_learning) {}

void InputMethodContextImplWayland::SetGrammarFragmentAtCursor(
    const ui::GrammarFragment& fragment) {}

void InputMethodContextImplWayland::SetAutocorrectInfo(
    const gfx::Range& autocorrect_range,
    const gfx::Rect& autocorrect_bounds) {}

void InputMethodContextImplWayland::SetCursorLocation(const gfx::Rect&) {}

void InputMethodContextImplWayland::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& selection_range) {
  NOTIMPLEMENTED_LOG_ONCE();
}

VirtualKeyboardController*
InputMethodContextImplWayland::GetVirtualKeyboardController() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

void InputMethodContextImplWayland::Commit(const std::string& text) {
  std::u16string string_committed = base::IsStringUTF8(text) ?
      base::UTF8ToUTF16(text) : base::ASCIIToUTF16(text);

  delegate_->OnCommit(string_committed);
}

void InputMethodContextImplWayland::PreeditChanged(const std::string& text,
                                                   const std::string& commit) {
  ui::CompositionText composition_text;
  if (base::IsStringUTF8(text)) {
    composition_text.text = base::UTF8ToUTF16(text);
    composition_text.selection = gfx::Range(0, composition_text.text.length());
    composition_text.ime_text_spans.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, 0, composition_text.text.length(),
        ui::ImeTextSpan::Thickness::kNone,
        ui::ImeTextSpan::UnderlineStyle::kNone, kPreeditHighlightColor));
  } else {
    composition_text.text = base::ASCIIToUTF16(text);
  }

  delegate_->OnPreeditChanged(composition_text);
}

void InputMethodContextImplWayland::DeleteRange(int32_t index,
                                                uint32_t length) {
  delegate_->OnDeleteRange(index, length);
}

}  // namespace ui
