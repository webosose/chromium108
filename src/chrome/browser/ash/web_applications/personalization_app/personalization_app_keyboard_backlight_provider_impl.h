// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_

#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_keyboard_backlight_provider.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class PersonalizationAppKeyboardBacklightProviderImpl
    : public PersonalizationAppKeyboardBacklightProvider,
      public WallpaperControllerObserver {
 public:
  explicit PersonalizationAppKeyboardBacklightProviderImpl(
      content::WebUI* web_ui);

  PersonalizationAppKeyboardBacklightProviderImpl(
      const PersonalizationAppKeyboardBacklightProviderImpl&) = delete;
  PersonalizationAppKeyboardBacklightProviderImpl& operator=(
      const PersonalizationAppKeyboardBacklightProviderImpl&) = delete;

  ~PersonalizationAppKeyboardBacklightProviderImpl() override;

  // PersonalizationAppKeyboardBacklightProvider:
  void BindInterface(mojo::PendingReceiver<mojom::KeyboardBacklightProvider>
                         receiver) override;

  // mojom::PersonalizationAppKeyboardBacklightProvider:
  void SetKeyboardBacklightObserver(
      mojo::PendingRemote<mojom::KeyboardBacklightObserver> observer) override;

  void SetBacklightColor(mojom::BacklightColor backlight_color) override;

  void ShouldShowNudge(ShouldShowNudgeCallback callback) override;

  void HandleNudgeShown() override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

 private:
  // Notify webUI the current state of backlight color.
  void NotifyBacklightColorChanged();

  // Pointer to profile of user that opened personalization SWA. Not owned.
  raw_ptr<Profile> const profile_ = nullptr;

  mojo::Receiver<mojom::KeyboardBacklightProvider> keyboard_backlight_receiver_{
      this};

  mojo::Remote<mojom::KeyboardBacklightObserver>
      keyboard_backlight_observer_remote_;

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PROVIDER_IMPL_H_
