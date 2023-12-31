// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

class PasswordGenerationPopupController;

namespace views {
class StyledLabel;
}

class PasswordGenerationPopupViewViews : public autofill::AutofillPopupBaseView,
                                         public PasswordGenerationPopupView {
 public:
  PasswordGenerationPopupViewViews(
      base::WeakPtr<PasswordGenerationPopupController> controller,
      views::Widget* parent_widget);

  PasswordGenerationPopupViewViews(const PasswordGenerationPopupViewViews&) =
      delete;
  PasswordGenerationPopupViewViews& operator=(
      const PasswordGenerationPopupViewViews&) = delete;

  // PasswordGenerationPopupView implementation
  [[nodiscard]] bool Show() override;
  void Hide() override;
  void UpdateState() override;
  void UpdateGeneratedPasswordValue() override;
  [[nodiscard]] bool UpdateBoundsAndRedrawPopup() override;
  void PasswordSelectionUpdated() override;

 private:
  class GeneratedPasswordBox;
  ~PasswordGenerationPopupViewViews() override;

  // Creates all the children views and adds them into layout.
  void CreateLayoutAndChildren();

  // views:Views implementation.
  void OnThemeChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;

  // Sub view that displays the actual generated password.
  raw_ptr<GeneratedPasswordBox> password_view_ = nullptr;

  // The footer label.
  raw_ptr<views::StyledLabel> help_styled_label_ = nullptr;

  // Controller for this view. Weak reference.
  base::WeakPtr<PasswordGenerationPopupController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_POPUP_VIEW_VIEWS_H_
