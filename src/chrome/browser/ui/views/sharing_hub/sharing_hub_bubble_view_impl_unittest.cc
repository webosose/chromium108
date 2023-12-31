// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/ui/sharing_hub/fake_sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

void EnumerateDescendants(views::View* root,
                          std::vector<views::View*>& result) {
  result.push_back(root);
  for (auto* child : root->children()) {
    EnumerateDescendants(child, result);
  }
}

std::vector<views::View*> DescendantsMatchingPredicate(
    views::View* root,
    base::RepeatingCallback<bool(views::View*)> predicate) {
  std::vector<views::View*> descendants;
  std::vector<views::View*> result;

  EnumerateDescendants(root, descendants);
  std::copy_if(descendants.begin(), descendants.end(),
               std::back_inserter(result),
               [=](views::View* view) { return predicate.Run(view); });
  return result;
}

bool ViewHasClassName(const std::string& class_name, views::View* view) {
  return view->GetClassName() == class_name;
}

std::string AccessibleNameForView(views::View* view) {
  ui::AXNodeData data;
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  return data.GetStringAttribute(ax::mojom::StringAttribute::kName);
}

void Click(views::Button* button) {
  button->OnMousePressed(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(1, 1), gfx::Point(0, 0),
                     base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  button->OnMouseReleased(
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(1, 1), gfx::Point(0, 0),
                     base::TimeTicks::Now(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
}

const std::vector<sharing_hub::SharingHubAction> kFirstPartyActions = {
    {0, u"Feed to Dino", nullptr, true, gfx::ImageSkia(), "feed-to-dino"},
    {1, u"Reverse Star", nullptr, true, gfx::ImageSkia(), "reverse-star"},
    {2, u"Pastelify", nullptr, true, gfx::ImageSkia(), "pastelify"},
};

const std::vector<sharing_hub::SharingHubAction> kThirdPartyActions = {
    {10, u"Owlbook", nullptr, false, gfx::ImageSkia(), "owlbook"},
    {11, u"Robinspace", nullptr, false, gfx::ImageSkia(), "robinspace"},
    {12, u"Ducktok", nullptr, false, gfx::ImageSkia(), "ducktok"},
    {13, u"Pigeongram", nullptr, false, gfx::ImageSkia(), "pigeongram"},
};

}  // namespace

class SharingHubBubbleTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ = CreateTestWidget();
  }

  void TearDown() override {
    bubble_widget_->CloseNow();
    anchor_widget_->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  void ShowBubble() {
    auto bubble = std::make_unique<sharing_hub::SharingHubBubbleViewImpl>(
        anchor_widget_->GetRootView(),
        share::ShareAttempt(nullptr, u"Hello!",
                            GURL("https://www.chromium.org"), ui::ImageModel()),
        &controller_);
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    bubble_->Show(sharing_hub::SharingHubBubbleViewImpl::USER_GESTURE);
    bubble_widget_ = bubble_->GetWidget();
  }

  sharing_hub::SharingHubBubbleViewImpl* bubble() { return bubble_; }

  sharing_hub::FakeSharingHubBubbleController* controller() {
    return &controller_;
  }

  std::vector<sharing_hub::SharingHubBubbleActionButton*> GetActionButtons() {
    std::vector<views::View*> actions = DescendantsMatchingPredicate(
        bubble(),
        base::BindRepeating(&ViewHasClassName, "SharingHubBubbleActionButton"));
    std::vector<sharing_hub::SharingHubBubbleActionButton*> concrete_actions;
    std::transform(
        actions.begin(), actions.end(), std::back_inserter(concrete_actions),
        [](views::View* view) {
          return static_cast<sharing_hub::SharingHubBubbleActionButton*>(view);
        });
    return concrete_actions;
  }

 private:
  base::test::ScopedFeatureList feature_list_{share::kDesktopSharePreview};

  sharing_hub::SharingHubBubbleViewImpl* bubble_;
  testing::NiceMock<sharing_hub::FakeSharingHubBubbleController> controller_{
      kFirstPartyActions, kThirdPartyActions};

  std::unique_ptr<views::Widget> anchor_widget_;
  views::Widget* bubble_widget_;
};

TEST_F(SharingHubBubbleTest, AllFirstPartyActionsAppearInOrder) {
  ShowBubble();

  auto actions = GetActionButtons();
  ASSERT_GE(actions.size(), 3u);
  EXPECT_EQ(AccessibleNameForView(actions[0]), "Feed to Dino");
  EXPECT_EQ(AccessibleNameForView(actions[1]), "Reverse Star");
  EXPECT_EQ(AccessibleNameForView(actions[2]), "Pastelify");
}

TEST_F(SharingHubBubbleTest, AllThirdPartyActionsAppearInOrder) {
  ShowBubble();

  auto actions = GetActionButtons();
  ASSERT_GE(actions.size(), 7u);
  EXPECT_EQ(AccessibleNameForView(actions[3]), "Share link to Owlbook");
  EXPECT_EQ(AccessibleNameForView(actions[4]), "Share link to Robinspace");
  EXPECT_EQ(AccessibleNameForView(actions[5]), "Share link to Ducktok");
  EXPECT_EQ(AccessibleNameForView(actions[6]), "Share link to Pigeongram");
}

TEST_F(SharingHubBubbleTest, ClickingActionsCallsController) {
  ShowBubble();

  auto actions = GetActionButtons();
  ASSERT_GE(actions.size(), 3u);
  EXPECT_CALL(*controller(), OnActionSelected(2, true, testing::_));
  Click(actions[2]);
}
