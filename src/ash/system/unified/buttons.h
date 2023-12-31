// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_BUTTONS_H_
#define ASH_SYSTEM_UNIFIED_BUTTONS_H_

#include "ash/system/power/power_status.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

// This file carries the buttons that will be shared in both the revamped
// `QuickSettingsView` and the old `UnifiedSystemTrayView`.
//
// TODO(crbug/1370847) When the old`UnifiedSystemTrayView` is removed, these
// buttons should be moved to the cc files that use them.

namespace views {
class Button;
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {

class UnifiedSystemTrayController;

// A base class for both `BatteryLabelView` and `BatteryIconView`. It updates by
// observing PowerStatus.
class BatteryInfoViewBase : public views::Button, public PowerStatus::Observer {
 public:
  METADATA_HEADER(BatteryInfoViewBase);

  explicit BatteryInfoViewBase(UnifiedSystemTrayController* controller);
  BatteryInfoViewBase(const BatteryInfoViewBase&) = delete;
  BatteryInfoViewBase& operator=(const BatteryInfoViewBase&) = delete;
  ~BatteryInfoViewBase() override;

  // Updates the subclass view's ui when `OnPowerStatusChanged`.
  virtual void Update() = 0;

 private:
  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;
};

// A view that shows battery status, which is used in the `QuickSettingsView`
// footer.
class BatteryLabelView : public BatteryInfoViewBase {
 public:
  METADATA_HEADER(BatteryLabelView);

  BatteryLabelView(UnifiedSystemTrayController* controller,
                   bool use_smart_charging_ui);
  BatteryLabelView(const BatteryLabelView&) = delete;
  BatteryLabelView& operator=(const BatteryLabelView&) = delete;
  ~BatteryLabelView() override;

 private:
  // views::View:
  void OnThemeChanged() override;

  // BatteryInfoViewBase:
  void Update() override;

  // Owned by this view, which is owned by views hierarchy.
  views::Label* percentage_ = nullptr;
  views::Label* separator_view_ = nullptr;
  views::Label* status_ = nullptr;

  //  If true, this view will only show the status and let the `BatteryIconView`
  //  show the rest. If false, the `percentage_` and separator will be visible.
  //  Smart charging means `ash::features::IsAdaptiveChargingEnabled()` and it
  //  is adaptive delaying charge.
  const bool use_smart_charging_ui_;
};

// A view that shows battery icon and charging state when smart charging is
// enabled, which is used in the `QuickSettingsView` footer.
class BatteryIconView : public BatteryInfoViewBase {
 public:
  METADATA_HEADER(BatteryIconView);

  explicit BatteryIconView(UnifiedSystemTrayController* controller);
  BatteryIconView(const BatteryIconView&) = delete;
  BatteryIconView& operator=(const BatteryIconView&) = delete;
  ~BatteryIconView() override;

 private:
  // views::View:
  void OnThemeChanged() override;

  // BatteryInfoViewBase:
  void Update() override;

  // Builds the battery icon image.
  void ConfigureIcon();

  // Owned by this view, which is owned by views hierarchy.
  views::Label* percentage_ = nullptr;
  views::ImageView* battery_image_ = nullptr;
};
}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_BUTTONS_H_
