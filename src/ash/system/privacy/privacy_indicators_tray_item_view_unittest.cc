// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kPrivacyIndicatorsViewExpandedShorterSideSize = 24;
const int kPrivacyIndicatorsViewExpandedLongerSideSize = 50;
const int kPrivacyIndicatorsViewSize = 8;

// Get the expected size in expand animation, given the animation value.
int GetExpectedSizeInExpandAnimation(double progress) {
  return kPrivacyIndicatorsViewExpandedLongerSideSize *
         gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100, progress);
}

// Get the expected size in shrink animation, given the animation value.
int GetExpectedSizeInShrinkAnimation(bool for_longer_side, double progress) {
  double animation_value =
      gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100, progress);
  int begin_size = for_longer_side
                       ? kPrivacyIndicatorsViewExpandedLongerSideSize
                       : kPrivacyIndicatorsViewExpandedShorterSideSize;
  return begin_size -
         (begin_size - kPrivacyIndicatorsViewSize) * animation_value;
}

// Get the expected tooltip text, given the string for camera/mic access and
// screen share.
std::u16string GetExpectedTooltipText(std::u16string cam_mic_status,
                                      std::u16string screen_share_status) {
  if (cam_mic_status.empty())
    return screen_share_status;

  if (screen_share_status.empty())
    return cam_mic_status;

  return l10n_util::GetStringFUTF16(IDS_PRIVACY_INDICATORS_VIEW_TOOLTIP,
                                    {cam_mic_status, screen_share_status},
                                    /*offsets=*/nullptr);
}

}  // namespace

namespace ash {

class PrivacyIndicatorsTrayItemViewTest : public AshTestBase {
 public:
  PrivacyIndicatorsTrayItemViewTest() = default;
  PrivacyIndicatorsTrayItemViewTest(const PrivacyIndicatorsTrayItemViewTest&) =
      delete;
  PrivacyIndicatorsTrayItemViewTest& operator=(
      const PrivacyIndicatorsTrayItemViewTest&) = delete;
  ~PrivacyIndicatorsTrayItemViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kPrivacyIndicators);

    AshTestBase::SetUp();
    privacy_indicators_view_ =
        std::make_unique<PrivacyIndicatorsTrayItemView>(GetPrimaryShelf());
  }

  std::u16string GetTooltipText() {
    return privacy_indicators_view_->GetTooltipText(gfx::Point());
  }

  views::BoxLayout* GetLayoutManager(
      PrivacyIndicatorsTrayItemView* privacy_indicators_view) {
    return privacy_indicators_view->layout_manager_;
  }

  void AnimateToValue(gfx::LinearAnimation* animation, double animation_value) {
    EXPECT_TRUE(animation->is_animating());
    animation->SetCurrentValue(animation_value);
    privacy_indicators_view_->AnimationProgressed(animation);
  }

  // Set `privacy_indicators_view_` to be visible and perform animation.
  void SetViewVisibleWithAnimation() {
    privacy_indicators_view()->SetVisible(true);
    privacy_indicators_view_->PerformVisibilityAnimation(/*visible=*/true);
  }

 protected:
  PrivacyIndicatorsTrayItemView* privacy_indicators_view() {
    return privacy_indicators_view_.get();
  }

  views::ImageView* camera_icon() {
    return privacy_indicators_view_->camera_icon_;
  }
  views::ImageView* microphone_icon() {
    return privacy_indicators_view_->microphone_icon_;
  }
  views::ImageView* screen_share_icon() {
    return privacy_indicators_view_->screen_share_icon_;
  }

  gfx::LinearAnimation* expand_animation() {
    return privacy_indicators_view_->expand_animation_.get();
  }

  PrivacyIndicatorsTrayItemView::AnimationState animation_state() {
    return privacy_indicators_view_->animation_state_;
  }

  gfx::LinearAnimation* longer_side_shrink_animation() {
    return privacy_indicators_view_->longer_side_shrink_animation_.get();
  }

  gfx::LinearAnimation* shorter_side_shrink_animation() {
    return privacy_indicators_view_->shorter_side_shrink_animation_.get();
  }

 private:
  std::unique_ptr<PrivacyIndicatorsTrayItemView> privacy_indicators_view_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacyIndicatorsTrayItemViewTest, IconsVisibility) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, ScreenShareIconsVisibility) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(screen_share_icon()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  // Test screen share showing up with other icons.
  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/true);
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());
  EXPECT_TRUE(screen_share_icon()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());
  EXPECT_FALSE(screen_share_icon()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, TooltipText) {
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/std::u16string(),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/false);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                       IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/true);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                       IDS_PRIVACY_NOTIFICATION_TITLE_MIC),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/true);
  EXPECT_EQ(
      GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                 IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC),
                             /*screen_share_status=*/std::u16string()),
      GetTooltipText());

  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/false);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/std::u16string(),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_EQ(GetExpectedTooltipText(
                /*cam_mic_status=*/std::u16string(),
                /*screen_share_status=*/l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE)),
            GetTooltipText());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, ShelfAlignmentChanged) {
  auto* privacy_indicators_view =
      GetPrimaryUnifiedSystemTray()->privacy_indicators_view();

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(views::BoxLayout::Orientation::kVertical,
            GetLayoutManager(privacy_indicators_view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_EQ(views::BoxLayout::Orientation::kHorizontal,
            GetLayoutManager(privacy_indicators_view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(views::BoxLayout::Orientation::kVertical,
            GetLayoutManager(privacy_indicators_view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottomLocked);
  EXPECT_EQ(views::BoxLayout::Orientation::kHorizontal,
            GetLayoutManager(privacy_indicators_view)->GetOrientation());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, VisibilityAnimation) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  AnimateToValue(expand_animation(), progress);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(GetExpectedSizeInExpandAnimation(progress),
            privacy_indicators_view()->GetPreferredSize().width());

  expand_animation()->End();

  // When expand animation ends, the view will be in `kDwellInExpand` state.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kDwellInExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedLongerSideSize,
            privacy_indicators_view()->GetPreferredSize().width());

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  EXPECT_EQ(
      PrivacyIndicatorsTrayItemView::AnimationState::kOnlyLongerSideShrink,
      animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().width());

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kBothSideShrink,
            animation_state());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/false, progress),
      privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().width());

  longer_side_shrink_animation()->End();
  shorter_side_shrink_animation()->End();

  // When finish, the view should have the size of a dot.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().width());
}

// Same test as above, but with the side shelf (the longer and shorter side will
// be flipped).
TEST_F(PrivacyIndicatorsTrayItemViewTest, SideShelfVisibilityAnimation) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  AnimateToValue(expand_animation(), progress);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(GetExpectedSizeInExpandAnimation(progress),
            privacy_indicators_view()->GetPreferredSize().height());

  expand_animation()->End();

  // When expand animation ends, the view will be in `kDwellInExpand` state.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kDwellInExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedLongerSideSize,
            privacy_indicators_view()->GetPreferredSize().height());

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  EXPECT_EQ(
      PrivacyIndicatorsTrayItemView::AnimationState::kOnlyLongerSideShrink,
      animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().height());

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kBothSideShrink,
            animation_state());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/false, progress),
      privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().height());

  longer_side_shrink_animation()->End();
  shorter_side_shrink_animation()->End();

  // When finish, the view should have the size of a dot.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().height());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, StateChangeDuringAnimation) {
  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  AnimateToValue(expand_animation(), progress);

  // Update state in mid animation, shouldn't crash anything.
  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/false);

  expand_animation()->End();

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  // Update the state again, no crash expected.
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  // The view should become invisible immediately after setting these states.
  privacy_indicators_view()->Update(/*app_id=*/"app_id",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/false);
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, MultipleAppsAccess) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id1",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  privacy_indicators_view()->Update(/*app_id=*/"app_id2",
                                    /*is_camera_used=*/true,
                                    /*is_microphone_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  // Indicator should still show when removing 1 app.
  privacy_indicators_view()->Update(/*app_id=*/"app_id2",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  // Indicator should hide when removing all apps.
  privacy_indicators_view()->Update(/*app_id=*/"app_id1",
                                    /*is_camera_used=*/false,
                                    /*is_microphone_used=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

}  // namespace ash
