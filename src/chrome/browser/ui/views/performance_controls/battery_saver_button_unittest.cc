// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

class BatterySaverButtonTest : public TestWithBrowserView {
 public:
  BatterySaverButtonTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kBatterySaverModeAvailable);
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    environment_.SetUp(&local_state_);
    TestWithBrowserView::SetUp();
  }

  void TearDown() override {
    TestWithBrowserView::TearDown();
    environment_.TearDown();
  }

  void SetBatterySaverModeEnabled(bool enabled) {
    auto mode = enabled ? performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kEnabled
                        : performance_manager::user_tuning::prefs::
                              BatterySaverModeState::kDisabled;
    local_state_.SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(mode));
  }

  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  base::HistogramTester histogram_tester_;
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      environment_;
};

// Battery saver button should not be shown when the pref state for battery
// saver mode is ON and shown when the pref state is ON
TEST_F(BatterySaverButtonTest, ShouldButtonShowTest) {
  const BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(battery_saver_button->GetVisible());

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());
}

// Battery saver button has the correct tooltip and accessibility text
TEST_F(BatterySaverButtonTest, TooltipAccessibilityTextTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
            battery_saver_button->GetTooltipText(gfx::Point()));

  ui::AXNodeData ax_node_data;
  battery_saver_button->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP),
      ax_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Battery saver bubble should be shown when the toolbar button is clicked
// and dismissed when it is clicked again
TEST_F(BatterySaverButtonTest, ShowAndHideBubbleOnButtonPressTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());

  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      battery_saver_button->GetBubble()->GetWidget());
  test_api.NotifyClick(e);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  destroyed_waiter.Wait();
}

// Dismiss bubble if expanded when battery saver mode is deactivated
TEST_F(BatterySaverButtonTest, DismissBubbleWhenModeDeactivatedTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());

  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      battery_saver_button->GetBubble()->GetWidget());
  SetBatterySaverModeEnabled(false);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());
  destroyed_waiter.Wait();
  EXPECT_FALSE(battery_saver_button->GetVisible());
}

// Check if the element identifier is set correctly by the battery saver
// toolbar button
TEST_F(BatterySaverButtonTest, ElementIdentifierTest) {
  const views::View* battery_saver_button_view =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button_view, nullptr);

  const views::View* matched_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kBatterySaverButtonElementId, browser_view()->GetElementContext());

  EXPECT_EQ(battery_saver_button_view, matched_view);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnDialogDismissTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  test_api.NotifyClick(e);
  EXPECT_FALSE(battery_saver_button->IsBubbleShowing());

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kDismiss, 1);
}

TEST_F(BatterySaverButtonTest, LogMetricsOnTurnOffNowTest) {
  BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_NE(battery_saver_button, nullptr);

  SetBatterySaverModeEnabled(true);
  EXPECT_TRUE(battery_saver_button->GetVisible());

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(battery_saver_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(battery_saver_button->IsBubbleShowing());

  views::BubbleDialogModelHost* const bubble_dialog_host =
      battery_saver_button->GetBubble();
  EXPECT_TRUE(bubble_dialog_host);

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      bubble_dialog_host->GetWidget());
  bubble_dialog_host->Cancel();
  destroyed_waiter.Wait();

  GetHistogramTester()->ExpectUniqueSample(
      "PerformanceControls.BatterySaver.BubbleAction",
      BatterySaverBubbleActionType::kTurnOffNow, 1);
}

class BatterySaverButtonNoExperimentsAvailableTest
    : public TestWithBrowserView {
 public:
  BatterySaverButtonNoExperimentsAvailableTest() = default;
};

// When battery saver mode available feature is disabled the toolbar button
// should not be initialized
TEST_F(BatterySaverButtonNoExperimentsAvailableTest, ShouldNotShowTest) {
  const BatterySaverButton* battery_saver_button =
      browser_view()->toolbar()->battery_saver_button();
  EXPECT_EQ(battery_saver_button, nullptr);
}
