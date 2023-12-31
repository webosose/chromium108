// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "ui/compositor/throughput_tracker.h"

namespace gfx {
class LinearAnimation;
}

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {
class Shelf;

// A tray item which resides in the system tray, indicating to users that an app
// is currently accessing camera/microphone.
class ASH_EXPORT PrivacyIndicatorsTrayItemView : public TrayItemView {
 public:
  enum AnimationState {
    // No animation is running.
    kIdle,

    // `expand_animation_` is running.
    kExpand,

    // `expand_animation_` finishes but the the shrink animation hasn't started
    // yet. The view will dwell at its expanded size.
    kDwellInExpand,

    // Happens when `longer_side_shrink_animation_` already started but
    // `shorter_side_shrink_animation_` hasn't started yet.
    kOnlyLongerSideShrink,

    // Happens when both the 2 shrink animations are animating. Note that
    // `longer_side_shrink_animation_` ended before
    // `shorter_side_shrink_animation_`, and this state ends when
    // `shorter_side_shrink_animation_` ends.
    kBothSideShrink,
  };

  explicit PrivacyIndicatorsTrayItemView(Shelf* shelf);

  PrivacyIndicatorsTrayItemView(const PrivacyIndicatorsTrayItemView&) = delete;
  PrivacyIndicatorsTrayItemView& operator=(
      const PrivacyIndicatorsTrayItemView&) = delete;

  ~PrivacyIndicatorsTrayItemView() override;

  // Update the view according to the state of camara/microphone access.
  void Update(const std::string& app_id,
              bool is_camera_used,
              bool is_microphone_used);

  // Update the view according to the state of screen sharing.
  void UpdateScreenShareStatus(bool is_screen_sharing);

  // Update the view according to the shelf alignment.
  void UpdateAlignmentForShelf(Shelf* shelf);

  // TrayItemView:
  std::u16string GetTooltipText(const gfx::Point& point) const override;

 private:
  friend class PrivacyIndicatorsTrayItemViewTest;

  // TrayItemView:
  void PerformVisibilityAnimation(bool visible) override;
  void HandleLocaleChange() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  const char* GetClassName() const override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Specify whether camera/microphone is in used.
  bool IsCameraUsed() const;
  bool IsMicrophoneUsed() const;

  // Update the icons for the children views.
  void UpdateIcons();

  // Update the bounds insets based on shelf alignment.
  void UpdateBoundsInset();

  // Calculate the size of the view during shrink animation. We are calculating
  // for the longer side if `for_longer_side` is true, otherwise it is for
  // shorter side.
  int CalculateSizeDuringShrinkAnimation(bool for_longer_side) const;

  // Calculate the length of the longer size, based on `is_screen_sharing_`.
  int GetLongerSideLengthInExpandedMode() const;

  // Update the access status of `app_id` for the given `access_set`.
  void UpdateAccessStatus(const std::string& app_id,
                          bool is_accessed,
                          base::flat_set<std::string>& access_set);

  // Update the view's visibility based on camera/mic access and screen sharing
  // state.
  void UpdateVisibility();

  // End all 3 animations contained in this class.
  void EndAllAnimations();

  views::BoxLayout* layout_manager_ = nullptr;

  // Owned by the views hierarchy.
  views::ImageView* camera_icon_ = nullptr;
  views::ImageView* microphone_icon_ = nullptr;
  views::ImageView* screen_share_icon_ = nullptr;

  // Store the app_id(s) that are currently accessing camera/microphone.
  base::flat_set<std::string> use_camera_apps_;
  base::flat_set<std::string> use_microphone_apps_;

  // Keep track of the current screen sharing state.
  bool is_screen_sharing_ = false;

  // Keep track the current animation state during the multi-part animation.
  AnimationState animation_state_ = kIdle;

  // Animations for showing/expanding the view, then shrink it to be a dot.
  std::unique_ptr<gfx::LinearAnimation> expand_animation_;
  std::unique_ptr<gfx::LinearAnimation> longer_side_shrink_animation_;
  std::unique_ptr<gfx::LinearAnimation> shorter_side_shrink_animation_;

  // Timers for delaying shrink animations after `expand_animation_` is
  // completed.
  base::OneShotTimer longer_side_shrink_delay_timer_;
  base::OneShotTimer shorter_side_shrink_delay_timer_;

  // Measure animation smoothness metrics for all the animations.
  absl::optional<ui::ThroughputTracker> throughput_tracker_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_TRAY_ITEM_VIEW_H_
