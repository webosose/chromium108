// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_nudge_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

namespace {
KeyboardBacklightColorController* GetKeyboardBacklightColorController() {
  auto* keyboard_backlight_color_controller =
      ash::Shell::Get()->keyboard_backlight_color_controller();
  DCHECK(keyboard_backlight_color_controller);
  return keyboard_backlight_color_controller;
}
}  // namespace

PersonalizationAppKeyboardBacklightProviderImpl::
    PersonalizationAppKeyboardBacklightProviderImpl(content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {}

PersonalizationAppKeyboardBacklightProviderImpl::
    ~PersonalizationAppKeyboardBacklightProviderImpl() = default;

void PersonalizationAppKeyboardBacklightProviderImpl::BindInterface(
    mojo::PendingReceiver<mojom::KeyboardBacklightProvider> receiver) {
  keyboard_backlight_receiver_.reset();
  keyboard_backlight_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppKeyboardBacklightProviderImpl::
    SetKeyboardBacklightObserver(
        mojo::PendingRemote<mojom::KeyboardBacklightObserver> observer) {
  // May already be bound if user refreshes page.
  keyboard_backlight_observer_remote_.reset();
  keyboard_backlight_observer_remote_.Bind(std::move(observer));

  // Call it once to get the status of color preset.
  NotifyBacklightColorChanged();

  // Bind wallpaper observer now that rgb keyboard section is ready and visible
  // to users.
  if (!wallpaper_controller_observation_.IsObserving())
    wallpaper_controller_observation_.Observe(WallpaperController::Get());
  // Call it once to get the wallpaper extracted color.
  OnWallpaperColorsChanged();
}

void PersonalizationAppKeyboardBacklightProviderImpl::SetBacklightColor(
    mojom::BacklightColor backlight_color) {
  if (!ash::features::IsRgbKeyboardEnabled()) {
    mojo::ReportBadMessage(
        "Cannot call `SetBacklightColor()` without rgb keyboard enabled");
    return;
  }
  DVLOG(4) << __func__ << " backlight_color=" << backlight_color;
  LogKeyboardBacklightColor(backlight_color);
  GetKeyboardBacklightColorController()->SetBacklightColor(
      backlight_color, GetAccountId(profile_));
  GetKeyboardBacklightColorController()
      ->keyboard_backlight_color_nudge_controller()
      ->SetUserPerformedAction();

  NotifyBacklightColorChanged();
}

void PersonalizationAppKeyboardBacklightProviderImpl::ShouldShowNudge(
    ShouldShowNudgeCallback callback) {
  std::move(callback).Run(
      KeyboardBacklightColorNudgeController::ShouldShowWallpaperColorNudge());
}

void PersonalizationAppKeyboardBacklightProviderImpl::HandleNudgeShown() {
  KeyboardBacklightColorNudgeController::HandleWallpaperColorNudgeShown();
}

void PersonalizationAppKeyboardBacklightProviderImpl::
    OnWallpaperColorsChanged() {
  DCHECK(keyboard_backlight_observer_remote_.is_bound());
  keyboard_backlight_observer_remote_->OnWallpaperColorChanged(
      ConvertBacklightColorToSkColor(
          personalization_app::mojom::BacklightColor::kWallpaper));
}

void PersonalizationAppKeyboardBacklightProviderImpl::
    NotifyBacklightColorChanged() {
  DCHECK(keyboard_backlight_observer_remote_.is_bound());
  keyboard_backlight_observer_remote_->OnBacklightColorChanged(
      GetKeyboardBacklightColorController()->GetBacklightColor(
          GetAccountId(profile_)));
}

}  // namespace ash::personalization_app
