// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <memory>
#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr auto kPrivacyIndicatorsViewPadding = gfx::Insets::VH(4, 8);
const int kPrivacyIndicatorsViewSpacing = 2;
const int kPrivacyIndicatorsIconSize = 16;
const int kPrivacyIndicatorsViewExpandedShorterSideSize = 24;
const int kPrivacyIndicatorsViewExpandedLongerSideSize = 50;
const int kPrivacyIndicatorsViewExpandedWithScreenShareSize = 68;
const int kPrivacyIndicatorsViewSize = 8;

constexpr auto kDwellInExpandDuration = base::Milliseconds(1000);
constexpr auto kShorterSizeShrinkAnimationDelay =
    kDwellInExpandDuration + base::Milliseconds(133);
constexpr auto kSizeChangeAnimationDuration = base::Milliseconds(333);
constexpr auto kExpandAnimationDuration = base::Milliseconds(400);
constexpr auto kIconFadeInDelayDuration = base::Milliseconds(83);
constexpr auto kCameraIconFadeInDuration = base::Milliseconds(233);
constexpr auto kMicAndScreenshareFadeInDuration = base::Milliseconds(116);

void StartAnimation(gfx::LinearAnimation* animation) {
  if (!animation)
    return;

  // Stop any ongoing animation.
  animation->End();

  animation->Start();
}

void StartRecordAnimationSmoothness(
    views::Widget* widget,
    absl::optional<ui::ThroughputTracker>& tracker) {
  // `widget` may not exist in tests.
  if (!widget)
    return;

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(
      ash::metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
        base::UmaHistogramPercentage(
            "Ash.PrivacyIndicators.AnimationSmoothness", smoothness);
      })));
}

void StartReportLayerAnimationSmoothness(
    const std::string& animation_histogram_name,
    int smoothness) {
  // Only record animation smoothness if `animation_histogram_name` is given.
  if (animation_histogram_name.empty())
    return;
  base::UmaHistogramPercentage(animation_histogram_name, smoothness);
}

void FadeInView(views::View* view,
                base::TimeDelta duration,
                const std::string& animation_histogram_name) {
  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  // Stop any ongoing animation.
  if (view->layer()->GetAnimator()->is_animating())
    view->layer()->GetAnimator()->StopAnimating();

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          &StartReportLayerAnimationSmoothness, animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(view, 0.0f)
      .At(kIconFadeInDelayDuration)
      .SetDuration(duration)
      .SetOpacity(view, 1.0f);
}

}  // namespace

PrivacyIndicatorsTrayItemView::PrivacyIndicatorsTrayItemView(Shelf* shelf)
    : TrayItemView(shelf),
      expand_animation_(std::make_unique<gfx::LinearAnimation>(
          kExpandAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)),
      longer_side_shrink_animation_(std::make_unique<gfx::LinearAnimation>(
          kSizeChangeAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)),
      shorter_side_shrink_animation_(std::make_unique<gfx::LinearAnimation>(
          kSizeChangeAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)) {
  SetVisible(false);

  auto container_view = std::make_unique<views::View>();
  layout_manager_ =
      container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                                  views::BoxLayout::Orientation::kVertical),
          kPrivacyIndicatorsViewPadding, kPrivacyIndicatorsViewSpacing));
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Set up a solid color layer to paint the background color, then add a layer
  // to each child so that they are visible and can perform layer animation.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kPrivacyIndicatorsViewExpandedShorterSideSize / 2});

  auto add_icon_to_container = [&container_view]() {
    auto icon = std::make_unique<views::ImageView>();
    icon->SetPaintToLayer();
    icon->layer()->SetFillsBoundsOpaquely(false);
    icon->SetVisible(false);
    return container_view->AddChildView(std::move(icon));
  };

  camera_icon_ = add_icon_to_container();
  microphone_icon_ = add_icon_to_container();
  screen_share_icon_ = add_icon_to_container();

  AddChildView(std::move(container_view));

  UpdateIcons();
  TooltipTextChanged();
}

PrivacyIndicatorsTrayItemView::~PrivacyIndicatorsTrayItemView() = default;

void PrivacyIndicatorsTrayItemView::Update(const std::string& app_id,
                                           bool is_camera_used,
                                           bool is_microphone_used) {
  UpdateAccessStatus(app_id, /*is_accessed=*/is_camera_used, use_camera_apps_);
  UpdateAccessStatus(app_id,
                     /*is_accessed=*/is_microphone_used, use_microphone_apps_);

  UpdateVisibility();
  if (!GetVisible())
    return;

  camera_icon_->SetVisible(IsCameraUsed());
  microphone_icon_->SetVisible(IsMicrophoneUsed());
  TooltipTextChanged();
}

void PrivacyIndicatorsTrayItemView::UpdateScreenShareStatus(
    bool is_screen_sharing) {
  if (is_screen_sharing_ == is_screen_sharing)
    return;
  is_screen_sharing_ = is_screen_sharing;

  UpdateVisibility();
  screen_share_icon_->SetVisible(is_screen_sharing_);
  TooltipTextChanged();
}

void PrivacyIndicatorsTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  layout_manager_->SetOrientation(
      shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                              views::BoxLayout::Orientation::kVertical));
  UpdateBoundsInset();
}

std::u16string PrivacyIndicatorsTrayItemView::GetTooltipText(
    const gfx::Point& point) const {
  auto cam_and_mic_status = std::u16string();
  if (IsCameraUsed() && IsMicrophoneUsed()) {
    cam_and_mic_status = l10n_util::GetStringUTF16(
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC);
  } else if (IsCameraUsed()) {
    cam_and_mic_status =
        l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA);
  } else if (IsMicrophoneUsed()) {
    cam_and_mic_status =
        l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC);
  }

  auto screen_share_status =
      is_screen_sharing_
          ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE)
          : std::u16string();

  if (cam_and_mic_status.empty())
    return screen_share_status;

  if (screen_share_status.empty())
    return cam_and_mic_status;

  return l10n_util::GetStringFUTF16(IDS_PRIVACY_INDICATORS_VIEW_TOOLTIP,
                                    {cam_and_mic_status, screen_share_status},
                                    /*offsets=*/nullptr);
}

void PrivacyIndicatorsTrayItemView::PerformVisibilityAnimation(bool visible) {
  EndAllAnimations();

  if (!visible)
    return;

  // Start a multi-part animation:
  // 1. kExpand: Expands to the fully expanded state, showing all icons.
  // 2. kDwellInExpand: Then dwells at this size for `kDwellInExpandDuration`.
  // 3. kOnlyLongerSideShrink: After that, collapses the long side first.
  // 4. kBothSideShrink: Before the long side shrinks completely, collapses the
  // short side to the final size (a green dot).
  expand_animation_->Start();
  StartRecordAnimationSmoothness(GetWidget(), throughput_tracker_);

  // At the same time, fade in icons.
  if (camera_icon_->GetVisible()) {
    FadeInView(camera_icon_, kCameraIconFadeInDuration,
               "Ash.PrivacyIndicators.CameraIcon.AnimationSmoothness");
  }
  if (microphone_icon_->GetVisible()) {
    FadeInView(camera_icon_, kMicAndScreenshareFadeInDuration,
               "Ash.PrivacyIndicators.MicrophoneIcon.AnimationSmoothness");
  }
  if (screen_share_icon_->GetVisible()) {
    FadeInView(camera_icon_, kMicAndScreenshareFadeInDuration,
               "Ash.PrivacyIndicators.ScreenshareIcon.AnimationSmoothness");
  }
}

void PrivacyIndicatorsTrayItemView::HandleLocaleChange() {
  TooltipTextChanged();
}

gfx::Size PrivacyIndicatorsTrayItemView::CalculatePreferredSize() const {
  int shorter_side;
  int longer_side;

  switch (animation_state_) {
    case AnimationState::kIdle:
      return gfx::Size(kPrivacyIndicatorsViewSize, kPrivacyIndicatorsViewSize);
    case AnimationState::kExpand:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side =
          GetLongerSideLengthInExpandedMode() *
          gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100,
                                     expand_animation_->GetCurrentValue());
      break;
    case AnimationState::kDwellInExpand:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side = GetLongerSideLengthInExpandedMode();
      break;
    case AnimationState::kOnlyLongerSideShrink:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/true);
      break;
    case AnimationState::kBothSideShrink:
      shorter_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/false);
      longer_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/true);
      break;
  }
  // `GetWidget()` might be null in unit tests.
  auto* shelf = GetWidget() ? Shelf::ForWindow(GetWidget()->GetNativeWindow())
                            : Shell::GetPrimaryRootWindowController()->shelf();
  // The view is rotated 90 degree in side shelf.
  return shelf->PrimaryAxisValue(gfx::Size(longer_side, shorter_side),
                                 gfx::Size(shorter_side, longer_side));
}

void PrivacyIndicatorsTrayItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIcons();

  layer()->SetColor(
      GetColorProvider()->GetColor(ui::kColorAshPrivacyIndicatorsBackground));
}

void PrivacyIndicatorsTrayItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateBoundsInset();
}

views::View* PrivacyIndicatorsTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

const char* PrivacyIndicatorsTrayItemView::GetClassName() const {
  return "PrivacyIndicatorsTrayItemView";
}

void PrivacyIndicatorsTrayItemView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == expand_animation_.get()) {
    animation_state_ = AnimationState::kExpand;
  } else if (animation == longer_side_shrink_animation_.get() &&
             !shorter_side_shrink_animation_->is_animating()) {
    animation_state_ = AnimationState::kOnlyLongerSideShrink;
  } else {
    animation_state_ = AnimationState::kBothSideShrink;
  }

  PreferredSizeChanged();
}

void PrivacyIndicatorsTrayItemView::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation_state_ == AnimationState::kExpand) {
    // Start kDwellInExpand when `expand_animation_` just finished.
    animation_state_ = kDwellInExpand;
    PreferredSizeChanged();

    longer_side_shrink_delay_timer_.Start(
        FROM_HERE, kDwellInExpandDuration,
        base::BindOnce(&StartAnimation, longer_side_shrink_animation_.get()));

    shorter_side_shrink_delay_timer_.Start(
        FROM_HERE, kShorterSizeShrinkAnimationDelay,
        base::BindOnce(&StartAnimation, shorter_side_shrink_animation_.get()));
  }

  // `shorter_side_shrink_animation_` should be the last one that is running, so
  // switch the state back to kIdle when it ends.
  if (animation == shorter_side_shrink_animation_.get()) {
    animation_state_ = AnimationState::kIdle;

    if (throughput_tracker_) {
      // Reset `throughput_tracker_` to reset animation metrics recording.
      throughput_tracker_->Stop();
      throughput_tracker_.reset();
    }
  }

  UpdateBoundsInset();
}

void PrivacyIndicatorsTrayItemView::AnimationCanceled(
    const gfx::Animation* animation) {
  // Finish all animations if one is canceled.
  EndAllAnimations();

  UpdateBoundsInset();
}

bool PrivacyIndicatorsTrayItemView::IsCameraUsed() const {
  return !use_camera_apps_.empty();
}

bool PrivacyIndicatorsTrayItemView::IsMicrophoneUsed() const {
  return !use_microphone_apps_.empty();
}

void PrivacyIndicatorsTrayItemView::UpdateIcons() {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  camera_icon_->SetImage(gfx::CreateVectorIcon(
      kPrivacyIndicatorsCameraIcon, kPrivacyIndicatorsIconSize, icon_color));
  microphone_icon_->SetImage(
      gfx::CreateVectorIcon(kPrivacyIndicatorsMicrophoneIcon,
                            kPrivacyIndicatorsIconSize, icon_color));
  screen_share_icon_->SetImage(
      gfx::CreateVectorIcon(kPrivacyIndicatorsScreenShareIcon,
                            kPrivacyIndicatorsIconSize, icon_color));
}

void PrivacyIndicatorsTrayItemView::UpdateBoundsInset() {
  gfx::Rect bounds = GetLocalBounds();

  // `GetWidget()` might be null in unit tests.
  auto* shelf = GetWidget() ? Shelf::ForWindow(GetWidget()->GetNativeWindow())
                            : Shell::GetPrimaryRootWindowController()->shelf();

  // We set the bounds inset based on the shorter side of the view (the shorter
  // size changes based on shelf alignment).
  int shorter_side_inset = shelf->PrimaryAxisValue(height(), width()) -
                           shelf->PrimaryAxisValue(GetPreferredSize().height(),
                                                   GetPreferredSize().width());
  bounds.Inset(
      shelf->PrimaryAxisValue(gfx::Insets::VH(shorter_side_inset / 2, 0),
                              gfx::Insets::VH(0, shorter_side_inset / 2)));
  layer()->SetClipRect(bounds);
}

int PrivacyIndicatorsTrayItemView::CalculateSizeDuringShrinkAnimation(
    bool for_longer_side) const {
  auto* animation = for_longer_side ? longer_side_shrink_animation_.get()
                                    : shorter_side_shrink_animation_.get();

  double animation_value = gfx::Tween::CalculateValue(
      gfx::Tween::ACCEL_20_DECEL_100, animation->GetCurrentValue());
  int begin_size = for_longer_side
                       ? GetLongerSideLengthInExpandedMode()
                       : kPrivacyIndicatorsViewExpandedShorterSideSize;

  // The size shrink from `begin_size` to kPrivacyIndicatorsViewSize when
  // `animation_value` goes from 0 to 1, and here is the calculation for it.
  return begin_size -
         (begin_size - kPrivacyIndicatorsViewSize) * animation_value;
}

int PrivacyIndicatorsTrayItemView::GetLongerSideLengthInExpandedMode() const {
  // If all three icons are visible, the view should be longer.
  return IsCameraUsed() && IsMicrophoneUsed() && is_screen_sharing_
             ? kPrivacyIndicatorsViewExpandedWithScreenShareSize
             : kPrivacyIndicatorsViewExpandedLongerSideSize;
}

void PrivacyIndicatorsTrayItemView::UpdateAccessStatus(
    const std::string& app_id,
    bool is_accessed,
    base::flat_set<std::string>& access_set) {
  if (access_set.contains(app_id) == is_accessed)
    return;

  if (is_accessed)
    access_set.insert(app_id);
  else
    access_set.erase(app_id);
}

void PrivacyIndicatorsTrayItemView::UpdateVisibility() {
  // We only hide the view when all the sets are empty.
  SetVisible(IsCameraUsed() || IsMicrophoneUsed() || is_screen_sharing_);
}

void PrivacyIndicatorsTrayItemView::EndAllAnimations() {
  shorter_side_shrink_animation_->End();
  longer_side_shrink_animation_->End();
  expand_animation_->End();
  animation_state_ = AnimationState::kIdle;

  if (throughput_tracker_) {
    // Reset `throughput_tracker_` to reset animation metrics recording.
    throughput_tracker_->Stop();
    throughput_tracker_.reset();
  }
}

}  // namespace ash
