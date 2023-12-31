// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/grammar_fragment.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

///@name USE_NEVA_APPRUNTIME
///@{
#include "ui/base/ime/linux/neva/linux_input_method_context_neva.h"
///@}

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

struct CompositionText;
class KeyEvent;
struct ImeTextSpan;
class VirtualKeyboardController;

// An interface of input method context for input method frameworks on
// GNU/Linux and likes.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContext {
 public:
  virtual ~LinuxInputMethodContext() = default;

  // Dispatches the key event to an underlying IME.  Returns true if the key
  // event is handled, otherwise false.  A client must set the text input type
  // before dispatching a key event.
  virtual bool DispatchKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Returns whether the event is a peek key event.
  virtual bool IsPeekKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Tells the system IME for the cursor rect which is relative to the
  // client window rect.
  virtual void SetCursorLocation(const gfx::Rect& rect) = 0;

  // Tells the system IME the surrounding text around the cursor location.
  virtual void SetSurroundingText(const std::u16string& text,
                                  const gfx::Range& selection_range) = 0;

  // Tells the system IME the content type of the text input client is changed.
  virtual void SetContentType(TextInputType type,
                              TextInputMode mode,
                              uint32_t flags,
                              bool should_do_learning) = 0;

  // Sets grammar fragment at the cursor position. If not exists, sends a
  // fragment with empty range.
  virtual void SetGrammarFragmentAtCursor(
      const ui::GrammarFragment& fragment) = 0;

  // Tells Ash about the current autocorrect information.
  virtual void SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                                  const gfx::Rect& autocorrect_bounds) = 0;

  // Resets the context.  A client needs to call OnTextInputTypeChanged() again
  // before calling DispatchKeyEvent().
  virtual void Reset() = 0;

  // Called when the text input focus is about to change.
  virtual void WillUpdateFocus(TextInputClient* old_client,
                               TextInputClient* new_client) {}

  // Called when text input focus is changed.
  virtual void UpdateFocus(bool has_client,
                           TextInputType old_type,
                           TextInputType new_type) = 0;

  // Returns the corresponding VirtualKeyboardController instance.
  // Or nullptr, if not supported.
  virtual VirtualKeyboardController* GetVirtualKeyboardController() = 0;
};

// An interface of callback functions called from LinuxInputMethodContext.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContextDelegate
    ///@name USE_NEVA_APPRUNTIME
    ///@{
    : public NevaLinuxInputMethodContextDelegate
    ///@}
    {
 public:
  virtual ~LinuxInputMethodContextDelegate() {}

  // Commits the |text| to the text input client.
  virtual void OnCommit(const std::u16string& text) = 0;

  // Converts current composition text into final content.
  virtual void OnConfirmCompositionText(bool keep_selection) = 0;

  // Deletes the surrounding text around selection. |before| and |after|
  // are in UTF-16 code points.
  virtual void OnDeleteSurroundingText(size_t before, size_t after) = 0;

#if defined(USE_NEVA_APPRUNTIME)
  virtual void OnMarkToSendKeyPressEvent() = 0;
#endif

  // Sets the composition text to the text input client.
  virtual void OnPreeditChanged(const CompositionText& composition_text) = 0;

  // Cleans up a composition session and makes sure that the composition text is
  // cleared.
  virtual void OnPreeditEnd() = 0;

  // Prepares things for a new composition session.
  virtual void OnPreeditStart() = 0;

  // Sets the composition from the current text in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetPreeditRegion(const gfx::Range& range,
                                  const std::vector<ImeTextSpan>& spans) = 0;

  // Clears all the grammar fragments in |range|. All indices are measured in
  // UTF-16 code point.
  virtual void OnClearGrammarFragments(const gfx::Range& range) = 0;

  // Adds a new grammar marker according to |fragments|. Clients should show
  // some visual indications such as underlining. All indices are measured in
  // UTF-16 code point.
  virtual void OnAddGrammarFragment(const ui::GrammarFragment& fragment) = 0;

  // Sets the autocorrect range in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetAutocorrectRange(const gfx::Range& range) = 0;

  // Sets the virtual keyboard's occluded bounds in screen DIP.
  virtual void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
