// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_icon_view.h"

#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The size of the insets for the `count_label_`.
constexpr int kCountLabelInsetSize = 4;

// When fetching images from the app service, request one of size 64, which is a
// common cached image size. With a higher resolution it will look better after
// resizing.
constexpr int kAppIdImageSize = 64;

// Make the icons 27x27 so that they appear 26x26, to account for the
// invisible padding added when standardizing icons.
constexpr int kIconSize = 27;

// Size the default icon to 22x22 so that it appears 20x20 accounting for the
// invisible padding.
constexpr int kDefaultIconSize = 22;

// The size of the background the icon sits inside of.
constexpr int kIconViewSize = 28;

// Return the formatted string for `count`. If `count` is <=99, the string will
// be "+<count>". If `count` is >99, the string will be "+99". If `show_plus` is
// false, the string will be just the count.
std::u16string GetCountString(int count, bool show_plus) {
  if (show_plus) {
    return base::UTF8ToUTF16(count > 99 ? "+99"
                                        : base::StringPrintf("+%i", count));
  }
  return base::NumberToString16(count);
}

gfx::ImageSkia CreateResizedImageToIconSize(const gfx::ImageSkia& icon,
                                            bool is_default) {
  const int diameter = is_default ? kDefaultIconSize : kIconSize;
  return gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, gfx::Size(diameter, diameter));
}

}  // namespace

SavedDeskIconView::SavedDeskIconView(
    const ui::ColorProvider* incognito_window_color_provider,
    const std::string& icon_identifier,
    const std::string& app_title,
    int count,
    bool show_plus,
    base::OnceCallback<void()> on_icon_loaded)
    : icon_identifier_(icon_identifier),
      count_(count),
      on_icon_loaded_(std::move(on_icon_loaded)) {
  if (visible_count() || is_overflow_icon()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        ash::kColorAshControlBackgroundColorInactive,
        /*radius=*/kIconViewSize / 2.0f));
  }

  CreateChildViews(incognito_window_color_provider, app_title, show_plus);
}

SavedDeskIconView::SavedDeskIconView(int count, bool show_plus)
    : SavedDeskIconView(nullptr, "", "", count, show_plus, {}) {}

SavedDeskIconView::~SavedDeskIconView() = default;

void SavedDeskIconView::UpdateCount(int count) {
  DCHECK(is_overflow_icon());
  DCHECK(count_label_);
  count_ = count;
  count_label_->SetText(GetCountString(visible_count(), /*show_plus=*/true));
}

gfx::Size SavedDeskIconView::CalculatePreferredSize() const {
  int width = (icon_view_ ? kIconViewSize : 0);
  if (count_label_) {
    if (visible_count()) {
      width += std::max(kIconViewSize,
                        count_label_->CalculatePreferredSize().width());
    }
  }
  return gfx::Size(width, kIconViewSize);
}

void SavedDeskIconView::Layout() {
  if (icon_view_) {
    gfx::Size icon_preferred_size = icon_view_->CalculatePreferredSize();
    icon_view_->SetBoundsRect(gfx::Rect(
        base::ClampFloor((kIconViewSize - icon_preferred_size.width()) / 2.0),
        base::ClampFloor((kIconViewSize - icon_preferred_size.height()) / 2.0),
        icon_preferred_size.width(), icon_preferred_size.height()));
  }
  if (count_label_) {
    count_label_->SetBoundsRect(
        gfx::Rect(icon_view_ ? kIconViewSize : 0, 0,
                  width() - (icon_view_ ? kIconViewSize : 0), kIconViewSize));
  }
}

void SavedDeskIconView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (count_label_) {
    auto* color_provider = AshColorProvider::Get();
    count_label_->SetBackgroundColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    count_label_->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  // The default icon is theme dependent, so it needs to be reloaded.
  if (is_showing_default_icon_)
    LoadDefaultIcon();
}

void SavedDeskIconView::CreateChildViews(
    const ui::ColorProvider* incognito_window_color_provider,
    const std::string& app_title,
    bool show_plus) {
  if (visible_count() || is_overflow_icon()) {
    DCHECK(!count_label_);
    count_label_ =
        AddChildView(views::Builder<views::Label>()
                         .SetText(GetCountString(visible_count(), show_plus))
                         .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                             kCountLabelInsetSize, kCountLabelInsetSize,
                             kCountLabelInsetSize,
                             is_overflow_icon() ? kCountLabelInsetSize
                                                : 2 * kCountLabelInsetSize)))
                         .SetAutoColorReadabilityEnabled(false)
                         .Build());
  }

  if (is_overflow_icon())
    return;

  // Add the icon to the front so that it gets read out before `count_label_` by
  // spoken feedback.
  DCHECK(!icon_view_);
  icon_view_ = AddChildViewAt(views::Builder<RoundedImageView>()
                                  .SetCornerRadius(kIconSize / 2.0f)
                                  .Build(),
                              0);

  // First check if the `icon_identifier_` is a special value, i.e. NTP url or
  // incognito window. If it is, use the corresponding icon for the special
  // value.
  auto* delegate = Shell::Get()->desks_templates_delegate();
  absl::optional<gfx::ImageSkia> chrome_icon =
      delegate->MaybeRetrieveIconForSpecialIdentifier(
          icon_identifier_, incognito_window_color_provider);

  icon_view_->GetViewAccessibility().OverrideRole(ax::mojom::Role::kImage);
  if (!app_title.empty())
    icon_view_->GetViewAccessibility().OverrideName(app_title);

  // PWAs (e.g. Messages) should use icon identifier as they share the same app
  // id as Chrome and would return short name for app id as "Chromium" (see
  // https://crbug.com/1281394). This is unlike Chrome browser apps which should
  // use `app_id` as their icon identifiers have been stripped to avoid
  // duplicate favicons (see https://crbug.com/1281391).
  if (chrome_icon.has_value()) {
    icon_view_->SetImage(CreateResizedImageToIconSize(chrome_icon.value(),
                                                      /*is_default=*/false));
    return;
  }

  // It's not a special value so `icon_identifier_` is either a favicon or an
  // app id. If `icon_identifier_` is not a valid url then it's an app id.
  GURL potential_url{icon_identifier_};
  if (!potential_url.is_valid()) {
    delegate->GetIconForAppId(icon_identifier_, kAppIdImageSize,
                              base::BindOnce(&SavedDeskIconView::OnIconLoaded,
                                             weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  delegate->GetFaviconForUrl(icon_identifier_,
                             base::BindOnce(&SavedDeskIconView::OnIconLoaded,
                                            weak_ptr_factory_.GetWeakPtr()),
                             &cancelable_task_tracker_);
}

void SavedDeskIconView::OnIconLoaded(const gfx::ImageSkia& icon) {
  if (!icon.isNull()) {
    icon_view_->SetImage(
        CreateResizedImageToIconSize(icon, /*is_default=*/false));
    return;
  }

  LoadDefaultIcon();

  // Notify the icon container to update the icon order and visibility.
  if (parent() && on_icon_loaded_)
    std::move(on_icon_loaded_).Run();
}

void SavedDeskIconView::LoadDefaultIcon() {
  is_showing_default_icon_ = true;

  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  // Use a higher resolution image as it will look better after resizing.
  const int resource_id = native_theme && native_theme->ShouldUseDarkColors()
                              ? IDR_DEFAULT_FAVICON_DARK_64
                              : IDR_DEFAULT_FAVICON_64;
  icon_view_->SetImage(CreateResizedImageToIconSize(
      gfx::ImageSkiaOperations::CreateColorMask(
          ui::ResourceBundle::GetSharedInstance()
              .GetImageNamed(resource_id)
              .AsImageSkia(),
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)),
      /*is_default=*/true));
}

BEGIN_METADATA(SavedDeskIconView, views::View)
END_METADATA

}  // namespace ash
