// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_

#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/time_to_click_recorder.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class UnifiedSystemTray;
class UnifiedMessageCenterView;
class SystemShadow;

// Manages the bubble that contains UnifiedMessageCenterView.
// Shows the bubble on `ShowBubble()`, and closes the bubble on the destructor.
class ASH_EXPORT UnifiedMessageCenterBubble
    : public ScreenLayoutObserver,
      public TrayBubbleBase,
      public TrayBubbleView::Delegate,
      public TimeToClickRecorder::Delegate,
      public views::ViewObserver {
 public:
  explicit UnifiedMessageCenterBubble(UnifiedSystemTray* tray);

  UnifiedMessageCenterBubble(const UnifiedMessageCenterBubble&) = delete;
  UnifiedMessageCenterBubble& operator=(const UnifiedMessageCenterBubble&) =
      delete;

  ~UnifiedMessageCenterBubble() override;

  // Return the bounds of the bubble in the screen.
  gfx::Rect GetBoundsInScreen() const;

  // We need the code to show the bubble explicitly separated from the
  // contructor. This is to prevent trigerring the TrayEventFilter from within
  // the constructor. Doing so can cause a crash when the TrayEventFilter tries
  // to reference the message center bubble before it is fully instantiated.
  void ShowBubble();

  // Collapse the bubble to only have the notification bar visible.
  void CollapseMessageCenter();

  // Expand the bubble to show all notifications.
  void ExpandMessageCenter();

  // Move the message center bubble to keep it on top of the quick settings
  // widget whenever the quick settings widget is resized.
  void UpdatePosition();

  // Inform `UnifiedMessageCenterView` of focus being acquired. The oldest
  // notification should be focused if `reverse` is `true`. Otherwise, if
  // `reverse` is `false`, the newest notification should be focused.
  void FocusEntered(bool reverse);

  // Relinquish focus and transfer it to the quick settings widget.
  bool FocusOut(bool reverse);

  // Activate quick settings bubble. Used when the message center is going
  // invisible.
  void ActivateQuickSettingsBubble();

  // Returns true if notifications are shown.
  bool IsMessageCenterVisible();

  // Returns true if only StackedNotificationBar is visible.
  bool IsMessageCenterCollapsed();

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

  UnifiedMessageCenterView* message_center_view() {
    return message_center_view_;
  }

 private:
  class Border;

  // Check if the message center bubble should be collapsed or expanded.
  void UpdateBubbleState();

  // Calculate the height usable for the bubble.
  int CalculateAvailableHeight();

  // TimeToClickRecorder::Delegate:
  void RecordTimeToClick() override;

  UnifiedSystemTray* const tray_;
  std::unique_ptr<Border> border_;
  std::unique_ptr<SystemShadow> shadow_;

  views::Widget* bubble_widget_ = nullptr;
  TrayBubbleView* bubble_view_ = nullptr;
  UnifiedMessageCenterView* message_center_view_ = nullptr;
  std::unique_ptr<TimeToClickRecorder> time_to_click_recorder_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
