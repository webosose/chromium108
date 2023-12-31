// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"

#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The focusable items for the FocusGroup::kSelection group.
constexpr std::array<FineTunePosition, 9> kSelectionTabbingOrder = {
    FineTunePosition::kCenter,       FineTunePosition::kTopLeft,
    FineTunePosition::kTopCenter,    FineTunePosition::kTopRight,
    FineTunePosition::kRightCenter,  FineTunePosition::kBottomRight,
    FineTunePosition::kBottomCenter, FineTunePosition::kBottomLeft,
    FineTunePosition::kLeftCenter};

// We inset the `window_of_interest` by `kWindowOfInterestInset` and outset any
// other window by `kIntersectingWindowOutset` we intersect with it, so that the
// resulting points of intersections are inside the bounds of
// `window_of_interest` and outside the bounds of the intersecting window.
// `chromeos::kResizeOutsideBoundsSize` is used while outsetting the window as
// it is the size of the window's resize border, and located events on it are
// still targeted to the window even though they're outside the window's bounds.
constexpr int kIntersectingWindowOutset =
    chromeos::kResizeOutsideBoundsSize + 1;
constexpr int kWindowOfInterestInset = 1;

std::vector<aura::Window*> GetWindowListIgnoreModalForActiveDesk() {
  return Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
      DesksMruType::kActiveDesk);
}

views::Widget* GetCameraPreviewWidget() {
  return CaptureModeController::Get()
      ->camera_controller()
      ->camera_preview_widget();
}

CameraPreviewView* GetCameraPreviewView() {
  return CaptureModeController::Get()
      ->camera_controller()
      ->camera_preview_view();
}

// Returns true if the `value` is within the inclusive range of `low` and
// `high`.
bool InRange(int value, int low, int high) {
  return value >= low && value <= high;
}

// Returns a vector of intersection points of two bounds.
std::vector<gfx::Point> GetIntersectionPoints(const gfx::Rect& bounds_a,
                                              const gfx::Rect& bounds_b) {
  // Calculate the attributes for the `intersection`.
  const int intersection_x = std::max(bounds_a.x(), bounds_b.x());
  const int intersection_y = std::max(bounds_a.y(), bounds_b.y());
  const int intersection_right = std::min(bounds_a.right(), bounds_b.right());
  const int intersection_bottom =
      std::min(bounds_a.bottom(), bounds_b.bottom());
  const auto intersection = gfx::Rect(intersection_x, intersection_y,
                                      intersection_right - intersection_x,
                                      intersection_bottom - intersection_y);

  if (intersection.width() <= 0 || intersection.height() <= 0)
    return {};

  const std::vector<gfx::Point> candidate_points = {
      intersection.origin(), intersection.bottom_left(),
      intersection.bottom_right(), intersection.top_right()};

  // Iterate the corners of the `intersection` and check if the point falls on
  // the edge of the `intersection` and within the range of the edge.
  std::vector<gfx::Point> intersection_points;
  for (const auto& point : candidate_points) {
    if (point.x() == bounds_a.x() &&
        InRange(point.y(), bounds_a.y(), bounds_a.bottom())) {
      intersection_points.push_back(point);
    }
    if (point.x() == bounds_a.right() &&
        InRange(point.y(), bounds_a.y(), bounds_a.bottom())) {
      intersection_points.push_back(point);
    }
    if (point.y() == bounds_a.y() &&
        InRange(point.x(), bounds_a.x(), bounds_a.right())) {
      intersection_points.push_back(point);
    }
    if (point.y() == bounds_a.bottom() &&
        InRange(point.x(), bounds_a.x(), bounds_a.right())) {
      intersection_points.push_back(point);
    }
  }
  return intersection_points;
}

// Returns true if `window_of_interest` is fully occluded with no point on it
// selectable by hovering the mouse cursor over it, false otherwise. When
// calculating the intersection point, we always inset the bounds of
// `window_of_interest` and outset the bounds of other windows which the
// `window_of_interest` is being compared with to ensure that the intersection
// point falls within the bounds of `window_of_interest`.
bool IsWindowFullyOccluded(aura::Window* window_of_interest) {
  std::stack<gfx::Point> points_stack;
  // Create a set to track the points that have been calculated and inserted
  // into the `points_stack` so that they will not be populated again and cause
  // unnecessary infinite loop.
  base::flat_set<gfx::Point> visited_points;
  gfx::Rect window_of_interest_bounds = window_of_interest->GetBoundsInScreen();
  gfx::Rect insetted_win_of_interest_bounds = window_of_interest_bounds;
  insetted_win_of_interest_bounds.Inset(kWindowOfInterestInset);
  points_stack.push(insetted_win_of_interest_bounds.origin());
  points_stack.push(insetted_win_of_interest_bounds.top_right());
  points_stack.push(insetted_win_of_interest_bounds.bottom_left());
  points_stack.push(insetted_win_of_interest_bounds.bottom_right());

  // Create a map: the key is the intersection point, the value is the window
  // which the `window_of_interest` is being compared with to get the
  // intersection point.
  base::flat_map<gfx::Point, aura::Window*> point_to_intersecting_window_map;
  base::flat_set<aura::Window*> visited;
  // Create a map to track the windows that have been compared so that we won't
  // re-insert the intersection points of the two windows into the
  // `points_stack` again if it has been calculated and inserted before.
  base::flat_set<std::pair<aura::Window*, aura::Window*>>
      compared_window_pair_set;

  while (!points_stack.empty()) {
    const gfx::Point point = points_stack.top();
    points_stack.pop();
    auto* top_window =
        capture_mode_util::GetTopMostCapturableWindowAtPoint(point);
    if (top_window == nullptr)
      continue;
    if (top_window == window_of_interest)
      return false;

    auto outsetted_top_window_bounds = top_window->GetBoundsInScreen();
    outsetted_top_window_bounds.Inset(-kIntersectingWindowOutset);

    if (!visited.insert(top_window).second) {
      auto iter = point_to_intersecting_window_map.find(point);
      if (iter == point_to_intersecting_window_map.end())
        continue;
      // `top_window` has been visited before, so we have already intersected it
      // with `window_of_interest` and added the intersection points (if any) to
      // the `points_stack`. However, the current `point` may have resulted from
      // intersecting `window_of_interest` with a window other than the current
      // `top_window`. We need to intersect `top_window` and that other window
      // to get the intersection points that fall inside the bounds of
      // `window_of_interest` (if any) so that we can check those as well.
      auto* associated_window = iter->second;
      if (associated_window == top_window)
        continue;

      auto outsetted_associated_win_bounds =
          associated_window->GetBoundsInScreen();

      if (!compared_window_pair_set
               .insert(std::make_pair(associated_window, top_window))
               .second ||
          !compared_window_pair_set
               .insert(std::make_pair(top_window, associated_window))
               .second) {
        continue;
      }

      outsetted_associated_win_bounds.Inset(-kIntersectingWindowOutset);
      for (const auto& p : GetIntersectionPoints(
               outsetted_top_window_bounds, outsetted_associated_win_bounds)) {
        if (visited_points.insert(p).second &&
            window_of_interest_bounds.Contains(p)) {
          points_stack.push(p);
        }

        // We don't need to insert `p` into `point_to_intersecting_window_map`
        // here as `p` is not directly from the `window_of_interest`.
      }

      continue;
    }

    if (!compared_window_pair_set
             .insert(std::make_pair(window_of_interest, top_window))
             .second ||
        !compared_window_pair_set
             .insert(std::make_pair(top_window, window_of_interest))
             .second) {
      continue;
    }

    for (const auto& p : GetIntersectionPoints(
             outsetted_top_window_bounds, insetted_win_of_interest_bounds)) {
      DCHECK(window_of_interest_bounds.Contains(p));

      if (visited_points.insert(p).second) {
        point_to_intersecting_window_map.emplace(p, top_window);
        points_stack.push(p);
      }
    }
  }
  return true;
}

bool IsCaptureWindowSelectable(aura::Window* window) {
  if (WindowState::Get(window)->IsMinimized() || !window->IsVisible())
    return false;

  return !IsWindowFullyOccluded(window);
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler::HighlightableView:

std::unique_ptr<views::HighlightPathGenerator>
CaptureModeSessionFocusCycler::HighlightableView::CreatePathGenerator() {
  return nullptr;
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoFocus() {
  has_focus_ = true;

  views::View* view = GetView();
  DCHECK(view);

  // This is lazy initialization of the FocusRing effectively. This is only used
  // for children of HighlightableView, so it will not replace any other style
  // of FocusRing.
  if (!focus_ring_) {
    focus_ring_ = StyleUtil::SetUpFocusRingForView(view);
    // Use a custom focus predicate as the default one checks if |view| actually
    // has focus which won't be happening since our widgets are not activatable.
    focus_ring_->SetHasFocusPredicate(
        [&](views::View* view) { return view->GetVisible() && has_focus_; });

    auto path_generator = CreatePathGenerator();
    if (path_generator)
      focus_ring_->SetPathGenerator(std::move(path_generator));
  }

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();

  view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

  magnifier_utils::MaybeUpdateActiveMagnifierFocus(
      view->GetBoundsInScreen().CenterPoint());
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoBlur() {
  has_focus_ = false;

  if (!focus_ring_)
    return;

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();
}

void CaptureModeSessionFocusCycler::HighlightableView::ClickView() {
  views::View* view = GetView();
  DCHECK(view);

  views::Button* button = views::Button::AsButton(view);
  if (!button)
    return;
  button->AcceleratorPressed(ui::Accelerator(ui::VKEY_SPACE, /*modifiers=*/0));
}

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler::HighlightableWindow:

CaptureModeSessionFocusCycler::HighlightableWindow::HighlightableWindow(
    aura::Window* window,
    CaptureModeSession* session)
    : window_(window), session_(session) {
  DCHECK(window_);
  window_->AddObserver(this);
}

CaptureModeSessionFocusCycler::HighlightableWindow::~HighlightableWindow() {
  window_->RemoveObserver(this);
}

views::View* CaptureModeSessionFocusCycler::HighlightableWindow::GetView() {
  return nullptr;
}

void CaptureModeSessionFocusCycler::HighlightableWindow::PseudoFocus() {
  has_focus_ = true;

  DCHECK(window_);
  session_->HighlightWindowForTab(window_);

  // TODO(afakhry): Check with a11y team if we need to focus on a
  // different region of the window.
  magnifier_utils::MaybeUpdateActiveMagnifierFocus(
      window_->GetBoundsInScreen().origin());
}

void CaptureModeSessionFocusCycler::HighlightableWindow::PseudoBlur() {
  has_focus_ = false;
}

void CaptureModeSessionFocusCycler::HighlightableWindow::ClickView() {
  // A HighlightableWindow is not clickable.
}

void CaptureModeSessionFocusCycler::HighlightableWindow::OnWindowDestroying(
    aura::Window* window) {
  session_->focus_cycler_->highlightable_windows_.erase(window);
  // `this` will be deleted after the above operation.
}

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler:

CaptureModeSessionFocusCycler::CaptureModeSessionFocusCycler(
    CaptureModeSession* session)
    : groups_for_fullscreen_{FocusGroup::kNone, FocusGroup::kTypeSource,
                             FocusGroup::kCameraPreview,
                             FocusGroup::kSettingsMenu,
                             FocusGroup::kSettingsClose},
      groups_for_region_{FocusGroup::kNone,          FocusGroup::kTypeSource,
                         FocusGroup::kSelection,     FocusGroup::kCameraPreview,
                         FocusGroup::kCaptureButton, FocusGroup::kSettingsMenu,
                         FocusGroup::kSettingsClose},
      groups_for_window_{FocusGroup::kNone, FocusGroup::kTypeSource,
                         FocusGroup::kCaptureWindow, FocusGroup::kSettingsMenu,
                         FocusGroup::kSettingsClose},
      session_(session),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  for (auto* window : GetWindowListIgnoreModalForActiveDesk()) {
    if (!IsCaptureWindowSelectable(window))
      continue;
    highlightable_windows_.emplace(
        window, std::make_unique<HighlightableWindow>(window, session_));
  }
}

CaptureModeSessionFocusCycler::~CaptureModeSessionFocusCycler() = default;

void CaptureModeSessionFocusCycler::AdvanceFocus(bool reverse) {
  // Advancing focus while the settings menu is open will close the menu and
  // clear focus, unless the settings menu was opened using keyboard navigation.
  if (!settings_menu_opened_with_keyboard_nav_) {
    views::Widget* settings_widget = GetSettingsMenuWidget();
    if (settings_widget && settings_widget->IsVisible()) {
      session_->SetSettingsMenuShown(false);
      return;
    }
  }

  ClearCurrentVisibleFocus();

  FocusGroup previous_focus_group = current_focus_group_;
  const size_t previous_group_size = GetGroupSize(previous_focus_group);
  const size_t previous_focus_index = focus_index_;

  // Go to the next group if the next index is out of bounds for the current
  // group. Otherwise, update |focus_index_| depending on |reverse|.
  if (!reverse && (previous_group_size == 0u ||
                   previous_focus_index >= previous_group_size - 1u)) {
    current_focus_group_ = GetNextGroup(/*reverse=*/false);
    focus_index_ = 0u;
  } else if (reverse && previous_focus_index == 0u) {
    current_focus_group_ = GetNextGroup(/*reverse=*/true);
    // The size of FocusGroup::kCaptureWindow could be empty.
    focus_index_ = std::max(
        static_cast<int32_t>(GetGroupSize(current_focus_group_)) - 1, 0);
  } else {
    focus_index_ = reverse ? focus_index_ - 1u : focus_index_ + 1u;
  }
  scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
      GetA11yOverrideWindow());

  const std::vector<HighlightableView*> current_views =
      GetGroupItems(current_focus_group_);
  // If `reverse`, focus the HighlightableWindow first before moving the focus
  // to items inside it.
  if (reverse)
    MaybeFocusHighlightableWindow(current_views);

  // Focus the new item.
  if (!current_views.empty()) {
    DCHECK_LT(focus_index_, current_views.size());
    current_views[focus_index_]->PseudoFocus();
  }

  // Selection focus is drawn directly on a layer owned by |session_|. Notify
  // the layer to repaint if necessary.
  const bool current_group_is_selection =
      current_focus_group_ == FocusGroup::kSelection;
  const bool redraw_layer = previous_focus_group == FocusGroup::kSelection ||
                            current_group_is_selection;

  if (redraw_layer)
    session_->RepaintRegion();

  if (current_group_is_selection) {
    const gfx::Rect user_region =
        CaptureModeController::Get()->user_capture_region();
    if (user_region.IsEmpty())
      return;

    const auto fine_tune_position = GetFocusedFineTunePosition();
    DCHECK_NE(fine_tune_position, FineTunePosition::kNone);

    gfx::Point point_of_interest =
        fine_tune_position == FineTunePosition::kCenter
            ? user_region.CenterPoint()
            : capture_mode_util::GetLocationForFineTunePosition(
                  user_region, fine_tune_position);
    wm::ConvertPointToScreen(session_->current_root(), &point_of_interest);
    magnifier_utils::MaybeUpdateActiveMagnifierFocus(point_of_interest);

    return;
  }
}

void CaptureModeSessionFocusCycler::ClearFocus() {
  ClearCurrentVisibleFocus();

  if (current_focus_group_ == FocusGroup::kSelection)
    session_->RepaintRegion();

  current_focus_group_ = FocusGroup::kNone;
  focus_index_ = 0u;
}

bool CaptureModeSessionFocusCycler::HasFocus() const {
  return current_focus_group_ != FocusGroup::kNone;
}

bool CaptureModeSessionFocusCycler::OnSpacePressed() {
  if (current_focus_group_ == FocusGroup::kNone ||
      current_focus_group_ == FocusGroup::kSelection ||
      current_focus_group_ == FocusGroup::kPendingSettings) {
    return false;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  if (views.empty())
    return false;

  // If current focused view doesn't exist, return directly.
  if (!FindFocusedViewAndUpdateFocusIndex(views))
    return false;

  DCHECK(!views.empty());
  DCHECK_LT(focus_index_, views.size());
  HighlightableView* view = views[focus_index_];

  // Let the session handle the space key event if the region toggle button
  // currently has focus and we are already in region mode, as we still want to
  // create a default region in this case.
  CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
  if (view->GetView() ==
          bar_view->capture_source_view()->region_toggle_button() &&
      CaptureModeController::Get()->source() == CaptureModeSource::kRegion) {
    return false;
  }

  // Clicking on the settings button first clears current focus and moves us to
  // a temporary state. The next focus signal will navigate through the settings
  // items.
  if (view->GetView() == session_->capture_mode_bar_view_->settings_button()) {
    settings_menu_opened_with_keyboard_nav_ = true;
    ClearCurrentVisibleFocus();
    current_focus_group_ = FocusGroup::kPendingSettings;
    focus_index_ = 0u;
  }

  // ClickView comes last as it will destroy |this| if |view| is the close
  // button.
  view->ClickView();
  return true;
}

bool CaptureModeSessionFocusCycler::RegionGroupFocused() const {
  return current_focus_group_ == FocusGroup::kSelection ||
         current_focus_group_ == FocusGroup::kCaptureButton;
}

bool CaptureModeSessionFocusCycler::CaptureBarFocused() const {
  return current_focus_group_ == FocusGroup::kTypeSource ||
         current_focus_group_ == FocusGroup::kSettingsClose ||
         current_focus_group_ == FocusGroup::kPendingSettings;
}

bool CaptureModeSessionFocusCycler::CaptureLabelFocused() const {
  return current_focus_group_ == FocusGroup::kCaptureButton;
}

FineTunePosition CaptureModeSessionFocusCycler::GetFocusedFineTunePosition()
    const {
  if (current_focus_group_ != FocusGroup::kSelection)
    return FineTunePosition::kNone;
  return kSelectionTabbingOrder[focus_index_];
}

void CaptureModeSessionFocusCycler::OnCaptureLabelWidgetUpdated() {
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnSettingsMenuWidgetCreated() {
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  DCHECK(settings_menu_widget);
  settings_menu_widget_observeration_.Observe(settings_menu_widget);
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnWidgetClosing(views::Widget* widget) {
  OnWidgetDestroying(widget);
}

void CaptureModeSessionFocusCycler::OnWidgetDestroying(views::Widget* widget) {
  // Note that we implement both `OnWidgetClosing()` and `OnWidgetDestroying()`.
  // - `OnWidgetClosing()` is called synchronously when either `Close()` or
  //   `CloseNow()` are called on the widget.
  // - `OnWidgetDestroying()` is called:
  //     - Synchronously if `CloseNow()` is used.
  //     - Asynchronously if `Close()` is used.
  // - However, `OnWidgetClosing()` may never get called at all if the native
  //   window of the widget gets deleted without calling either `Close()` or
  //   `CloseNow()`. See https://crbug.com/1350743.
  // Implementing both let's us handle the closing synchronously via
  // `OnWidgetClosing()`, and avoid any crashes or UAFs if it was never called.
  if (!settings_menu_widget_observeration_.IsObserving())
    return;

  settings_menu_opened_with_keyboard_nav_ = false;
  settings_menu_widget_observeration_.Reset();

  // Return immediately if the widget is closing by the closing of `session_`.
  if (session_->is_shutting_down())
    return;
  // Remove focus if one of the settings related groups is currently
  // focused.
  if (current_focus_group_ == FocusGroup::kPendingSettings ||
      current_focus_group_ == FocusGroup::kSettingsMenu) {
    // When the settings menu is closed while focus is in or about to be in it,
    // we manually put the focus back on the settings button.
    current_focus_group_ = FocusGroup::kSettingsClose;
    focus_index_ = 0u;
    const auto highlightable_views = GetGroupItems(current_focus_group_);
    DCHECK_EQ(highlightable_views.size(), 2u);
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
        GetA11yOverrideWindow());
    highlightable_views[focus_index_]->PseudoFocus();
  }
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::ClearCurrentVisibleFocus() {
  // The settings menu widget may be destroyed while it has focus. No need to
  // clear focus in this case.
  if (current_focus_group_ == FocusGroup::kSettingsMenu &&
      !GetSettingsMenuWidget()) {
    return;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  if (views.empty())
    return;

  // If current focused view doesn't exist, return directly.
  if (!FindFocusedViewAndUpdateFocusIndex(views))
    return;

  DCHECK_LT(focus_index_, views.size());
  views[focus_index_]->PseudoBlur();
}

CaptureModeSessionFocusCycler::FocusGroup
CaptureModeSessionFocusCycler::GetNextGroup(bool reverse) const {
  if (current_focus_group_ == FocusGroup::kPendingSettings) {
    DCHECK(GetSettingsMenuWidget());
    return FocusGroup::kSettingsMenu;
  }

  const std::vector<FocusGroup>& groups_list = GetCurrentGroupList();
  const int increment = reverse ? -1 : 1;
  const auto iter = base::ranges::find(groups_list, current_focus_group_);
  DCHECK(iter != groups_list.end());
  size_t next_group_index = std::distance(groups_list.begin(), iter);
  const auto group_size = groups_list.size();
  do {
    next_group_index = (group_size + next_group_index + increment) % group_size;
  } while (!IsGroupAvailable(groups_list[next_group_index]));

  return groups_list[next_group_index];
}

const std::vector<CaptureModeSessionFocusCycler::FocusGroup>&
CaptureModeSessionFocusCycler::GetCurrentGroupList() const {
  switch (session_->controller_->source()) {
    case CaptureModeSource::kFullscreen:
      return groups_for_fullscreen_;
    case CaptureModeSource::kRegion:
      return groups_for_region_;
    case CaptureModeSource::kWindow:
      return groups_for_window_;
  }
}

bool CaptureModeSessionFocusCycler::IsGroupAvailable(FocusGroup group) const {
  switch (group) {
    case FocusGroup::kNone:
    case FocusGroup::kTypeSource:
    case FocusGroup::kSettingsClose:
    case FocusGroup::kPendingSettings:
      return true;
    case FocusGroup::kSelection:
    case FocusGroup::kCaptureButton: {
      // The selection UI and capture button are focusable only when the label
      // button of CaptureLabelView is visible.
      auto* widget = session_->capture_label_widget_.get();
      if (!widget)
        return false;
      auto* capture_label_view =
          static_cast<CaptureLabelView*>(widget->GetContentsView());
      return capture_label_view->label_button()->GetVisible();
    }
    case FocusGroup::kCaptureWindow:
      return session_->controller_->source() == CaptureModeSource::kWindow &&
             GetGroupSize(FocusGroup::kCaptureWindow) > 0;
    case FocusGroup::kSettingsMenu:
      return session_->capture_mode_settings_view_;
    case FocusGroup::kCameraPreview: {
      auto* camera_preview_widget = GetCameraPreviewWidget();
      return camera_preview_widget && camera_preview_widget->IsVisible();
    }
  }
}

size_t CaptureModeSessionFocusCycler::GetGroupSize(FocusGroup group) const {
  if (group == FocusGroup::kSelection)
    return 9u;
  return GetGroupItems(group).size();
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureModeSessionFocusCycler::GetGroupItems(FocusGroup group) const {
  std::vector<HighlightableView*> items;
  switch (group) {
    case FocusGroup::kNone:
    case FocusGroup::kSelection:
    case FocusGroup::kPendingSettings:
      break;
    case FocusGroup::kTypeSource: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      CaptureModeTypeView* type_view = bar_view->capture_type_view();
      CaptureModeSourceView* source_view = bar_view->capture_source_view();
      items = {type_view->image_toggle_button(),
               type_view->video_toggle_button(),
               source_view->fullscreen_toggle_button(),
               source_view->region_toggle_button(),
               source_view->window_toggle_button()};

      base::EraseIf(items,
                    [](CaptureModeSessionFocusCycler::HighlightableView* item) {
                      return !item || !item->GetView()->GetEnabled();
                    });
      break;
    }
    case FocusGroup::kCaptureButton: {
      views::Widget* widget = session_->capture_label_widget_.get();
      DCHECK(widget);
      items = {static_cast<CaptureLabelView*>(widget->GetContentsView())};
      break;
    }
    case FocusGroup::kCaptureWindow: {
      const std::vector<aura::Window*> windows =
          GetWindowListIgnoreModalForActiveDesk();
      if (!windows.empty()) {
        const std::vector<HighlightableView*> camera_items =
            GetGroupItems(FocusGroup::kCameraPreview);
        for (auto* window : windows) {
          auto iter = highlightable_windows_.find(window);
          if (iter != highlightable_windows_.end()) {
            items.push_back(iter->second.get());
            items.insert(items.end(), camera_items.begin(), camera_items.end());
          }
        }
      }
      break;
    }
    case FocusGroup::kSettingsClose: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      items = {bar_view->settings_button(), bar_view->close_button()};
      break;
    }
    case FocusGroup::kSettingsMenu: {
      CaptureModeSettingsView* settings_view =
          session_->capture_mode_settings_view_;
      DCHECK(settings_view);
      items = settings_view->GetHighlightableItems();
      break;
    }
    case FocusGroup::kCameraPreview: {
      auto* camera_preview_view = GetCameraPreviewView();
      if (camera_preview_view) {
        items.push_back(camera_preview_view);
        // The resize button is forced to be hidden if the camera preview is not
        // collapsible. Do not advance the focus to it in this case.
        if (camera_preview_view->is_collapsible())
          items.push_back(camera_preview_view->resize_button());
      }
      break;
    }
  }
  return items;
}

views::Widget* CaptureModeSessionFocusCycler::GetSettingsMenuWidget() const {
  return session_->capture_mode_settings_widget_.get();
}

aura::Window* CaptureModeSessionFocusCycler::GetA11yOverrideWindow() const {
  switch (current_focus_group_) {
    case FocusGroup::kCaptureButton:
      return session_->capture_label_widget()->GetNativeWindow();
    case FocusGroup::kSettingsMenu:
      return session_->capture_mode_settings_widget()->GetNativeWindow();
    case FocusGroup::kNone:
    case FocusGroup::kTypeSource:
    case FocusGroup::kSelection:
    case FocusGroup::kCaptureWindow:
    case FocusGroup::kSettingsClose:
    case FocusGroup::kPendingSettings:
      return session_->capture_mode_bar_widget()->GetNativeWindow();
    case FocusGroup::kCameraPreview:
      return GetCameraPreviewWidget()->GetNativeWindow();
  }
}

bool CaptureModeSessionFocusCycler::FindFocusedViewAndUpdateFocusIndex(
    std::vector<HighlightableView*> views) {
  // No need to update `focus_index_` if the corresponding view is focused now.
  if (focus_index_ < views.size() && views[focus_index_]->has_focus())
    return true;

  const size_t current_focus_index =
      base::ranges::find(
          views, true,
          &CaptureModeSessionFocusCycler::HighlightableView::has_focus) -
      views.begin();

  // If current focused view doesn't exist, return false;
  if (current_focus_index == views.size()) {
    // If `focus_index_` is out of bound, update it to the last index of the
    // `views`.
    if (focus_index_ >= views.size())
      focus_index_ = views.size() - 1;
    return false;
  }

  // Update `focus_index_` to ensure it's up to date, since highlightable views
  // of `current_focus_group_` can be updated during keyboard navigation, for
  // example, the custom folder option can be added or removed via the select
  // folder menu item.
  focus_index_ = current_focus_index;
  return true;
}

void CaptureModeSessionFocusCycler::UpdateA11yAnnotation() {
  std::vector<views::Widget*> a11y_widgets;

  // If the bar widget is not available, then this is called while shutting
  // down the capture mode session.
  views::Widget* bar_widget = session_->capture_mode_bar_widget_.get();
  if (bar_widget)
    a11y_widgets.push_back(bar_widget);

  // Add the label widget only if the button is visible.
  views::Widget* label_widget = session_->capture_label_widget_.get();
  if (label_widget &&
      static_cast<CaptureLabelView*>(label_widget->GetContentsView())
          ->label_button()
          ->GetVisible()) {
    a11y_widgets.push_back(label_widget);
  }

  // Add the settings widget if it exists.
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  if (settings_menu_widget)
    a11y_widgets.push_back(settings_menu_widget);

  // Helper to update |target|'s a11y focus with |previous| and |next|, which
  // can be null.
  auto update_a11y_widget_focus =
      [](views::Widget* target, views::Widget* previous, views::Widget* next) {
        DCHECK(target);
        auto* contents_view = target->GetContentsView();
        auto& view_a11y = contents_view->GetViewAccessibility();
        view_a11y.OverridePreviousFocus(previous);
        view_a11y.OverrideNextFocus(next);
        contents_view->NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged,
                                                true);
      };

  // If there is only one widget left, clear the focus overrides so that they
  // do not point to deleted objects.
  if (a11y_widgets.size() == 1u) {
    update_a11y_widget_focus(a11y_widgets[0], nullptr, nullptr);
    return;
  }

  const int size = a11y_widgets.size();
  for (int i = 0; i < size; ++i) {
    const int previous_index = (i + size - 1) % size;
    const int next_index = (i + 1) % size;
    update_a11y_widget_focus(a11y_widgets[i], a11y_widgets[previous_index],
                             a11y_widgets[next_index]);
  }
}

void CaptureModeSessionFocusCycler::MaybeFocusHighlightableWindow(
    const std::vector<HighlightableView*>& current_views) {
  if (current_focus_group_ != FocusGroup::kCaptureWindow)
    return;

  const std::vector<HighlightableView*> camera_preview_group_items =
      GetGroupItems(FocusGroup::kCameraPreview);
  if (camera_preview_group_items.empty())
    return;

  DCHECK(!current_views.empty());
  // Call HighlightableWindow::PseudoFocus() to highlight the window first
  // before moving the focus to the last focusable item inside the camera
  // preview. This will set the window as the current selected window, which
  // will move the camera preview inside it.
  if (current_views[focus_index_] == camera_preview_group_items.back()) {
    const size_t focusable_items_in_a_window =
        1 + camera_preview_group_items.size();
    const size_t window_index = (focus_index_ / focusable_items_in_a_window) *
                                focusable_items_in_a_window;
    current_views[window_index]->PseudoFocus();
  }
}

}  // namespace ash
