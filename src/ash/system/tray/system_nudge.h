// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/system_nudge_label.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
class View;
}  // namespace views

namespace ash {

// Creates and manages the nudge widget and its contents view for a contextual
// system nudge. The nudge displays an icon and a label view in a shelf-colored
// system bubble with rounded corners.
class ASH_EXPORT SystemNudge : public ShelfObserver, ShellObserver {
 public:
  SystemNudge(const std::string& name,
              NudgeCatalogName catalog_name,
              int icon_size,
              int icon_label_spacing,
              int nudge_padding,
              AshColorProvider::ContentLayerType icon_color_layer_type =
                  AshColorProvider::ContentLayerType::kIconColorPrimary);
  SystemNudge(const SystemNudge&) = delete;
  SystemNudge& operator=(const SystemNudge&) = delete;
  ~SystemNudge() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // Displays the nudge.
  void Show();

  // Closes the nudge.
  void Close();

  views::Widget* widget() { return widget_.get(); }

 protected:
  // Each SystemNudge subclass must override these methods to customize
  // their nudge by creating a label and getting an icon specific to the feature
  // being nudged. These will be called only when needed by Show().

  // Creates and initializes a view representing the label for the nudge.
  virtual std::unique_ptr<SystemNudgeLabel> CreateLabelView() const = 0;

  // Gets the VectorIcon shown to the side of the label for the nudge.
  virtual const gfx::VectorIcon& GetIcon() const = 0;

  // Gets the string to announce for accessibility. This will be used if a
  // screen reader is enabled when the view is shown.
  virtual std::u16string GetAccessibilityText() const = 0;

 private:
  class SystemNudgeView;

  struct SystemNudgeParams {
    // The name for the widget.
    std::string name;
    // The catalog name for the system nudge.
    NudgeCatalogName catalog_name;
    // The size of the icon.
    int icon_size;
    // The size of the space between icon and label.
    int icon_label_spacing;
    // The padding which separates the nudge's border with its inner contents.
    int nudge_padding;
    // The color of the icon.
    AshColorProvider::ContentLayerType icon_color_layer_type =
        AshColorProvider::ContentLayerType::kIconColorPrimary;
  };

  // Calculate and set widget bounds based on a fixed width and a variable
  // height to correctly fit the label contents.
  void CalculateAndSetWidgetBounds();

  views::UniqueWidgetPtr widget_;

  SystemNudgeView* nudge_view_ = nullptr;  // not_owned

  aura::Window* const root_window_;

  SystemNudgeParams params_;

  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
  base::ScopedObservation<Shell,
                          ShellObserver,
                          &Shell::AddShellObserver,
                          &Shell::RemoveShellObserver>
      shell_observation_{this};

  base::WeakPtrFactory<SystemNudge> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_
