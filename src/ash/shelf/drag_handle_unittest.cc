// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_handle.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

namespace {

enum class TestAccessibilityFeature {
  kTabletModeShelfNavigationButtons,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests drag handle functionalities with number of accessibility setting
// enabled.
class DragHandleTest
    : public AshTestBase,
      public ::testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  DragHandleTest() = default;
  ~DragHandleTest() override = default;

  const DragHandle* drag_handle() const {
    return GetPrimaryShelf()->shelf_widget()->GetDragHandle();
  }

  void ClickDragHandle() {
    gfx::Point center = drag_handle()->GetBoundsInScreen().CenterPoint();
    GetEventGenerator()->MoveMouseTo(center);
    GetEventGenerator()->ClickLeftButton();
  }

  void SetTestA11yFeatureEnabled(bool enabled) {
    switch (GetParam()) {
      case TestAccessibilityFeature::kTabletModeShelfNavigationButtons:
        Shell::Get()
            ->accessibility_controller()
            ->SetTabletModeShelfNavigationButtonsEnabled(enabled);
        break;
      case TestAccessibilityFeature::kSpokenFeedback:
        Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
            enabled, A11Y_NOTIFICATION_NONE);
        break;
      case TestAccessibilityFeature::kAutoclick:
        Shell::Get()->accessibility_controller()->autoclick().SetEnabled(
            enabled);
        break;
      case TestAccessibilityFeature::kSwitchAccess:
        Shell::Get()->accessibility_controller()->switch_access().SetEnabled(
            enabled);
        Shell::Get()
            ->accessibility_controller()
            ->DisableSwitchAccessDisableConfirmationDialogTesting();
        break;
    }
  }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    DragHandleTest,
    ::testing::Values(
        TestAccessibilityFeature::kTabletModeShelfNavigationButtons,
        TestAccessibilityFeature::kSpokenFeedback,
        TestAccessibilityFeature::kAutoclick,
        TestAccessibilityFeature::kSwitchAccess));

TEST_P(DragHandleTest, AccessibilityFeaturesEnabled) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  UpdateDisplay("800x700");
  // Create a widget to transition to the in-app shelf.
  TestWidgetBuilder()
      .SetTestWidgetDelegate()
      .SetBounds(gfx::Rect(0, 0, 800, 800))
      .BuildOwnedByNativeWidget();

  EXPECT_TRUE(drag_handle()->GetVisible());

  // By default, drag handle should not function as a button.
  EXPECT_FALSE(drag_handle()->GetEnabled());

  // If a11y feature is enabled, the drag handle button should behave like a
  // button.
  SetTestA11yFeatureEnabled(true /*enabled*/);
  EXPECT_TRUE(drag_handle()->GetEnabled());

  EXPECT_EQ(HotseatState::kHidden,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // Click on the drag handle should extend the hotseat.
  ClickDragHandle();
  EXPECT_EQ(HotseatState::kExtended,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // Click again should hide the hotseat.
  ClickDragHandle();
  EXPECT_EQ(HotseatState::kHidden,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // Exit a11y feature should disable drag handle.
  SetTestA11yFeatureEnabled(false /*enabled*/);
  EXPECT_FALSE(drag_handle()->GetEnabled());
}

}  // namespace ash
