// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/gfx/vector_icon_types.h"

class Browser;
class Profile;

// This icon appears in the location bar when the current page qualifies for
// price tracking. Upon clicking, it shows a bubble where the user can choose to
// track or untrack the current page.
class PriceTrackingIconView : public PageActionIconView {
 public:
  PriceTrackingIconView(IconLabelBubbleView::Delegate* parent_delegate,
                        Delegate* delegate,
                        Browser* browser);
  ~PriceTrackingIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  void ForceVisibleForTesting(bool is_tracking_price);
  const std::u16string& GetIconLabelForTesting();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void EnablePriceTracking(bool enable);
  void SetVisualState(bool enable);
  void OnPriceTrackingServerStateUpdated(bool success);
  bool ShouldShow();
  bool IsPriceTracking() const;
  bool ShouldShowFirstUseExperienceBubble() const;
  void MaybeShowPageActionLabel();
  void HidePageActionLabel();

  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  PriceTrackingBubbleCoordinator bubble_coordinator_;

  raw_ptr<const gfx::VectorIcon> icon_;
  std::u16string tooltip_text_and_accessibleName_;

  // Animates out the price tracking icon label after a fixed period of time.
  // This keeps the label visible for long enough to give users an opportunity
  // to read the label text.
  base::RetainingOneShotTimer animate_out_timer_;
  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;

  base::WeakPtrFactory<PriceTrackingIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_
