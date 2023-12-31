// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/scoped_observation.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"

class PrefRegistrySimple;

namespace ash {

class KeyboardBacklightColorNudgeController;

// Controller to manage keyboard backlight colors.
class ASH_EXPORT KeyboardBacklightColorController
    : public SessionObserver,
      public WallpaperControllerObserver {
 public:
  KeyboardBacklightColorController();

  KeyboardBacklightColorController(const KeyboardBacklightColorController&) =
      delete;
  KeyboardBacklightColorController& operator=(
      const KeyboardBacklightColorController&) = delete;

  ~KeyboardBacklightColorController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the keyboard backlight color for the user with |account_id|.
  void SetBacklightColor(
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  // Returns the currently set backlight color for user with |account_id|.
  personalization_app::mojom::BacklightColor GetBacklightColor(
      const AccountId& account_id);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

  KeyboardBacklightColorNudgeController*
  keyboard_backlight_color_nudge_controller() {
    return keyboard_backlight_color_nudge_controller_.get();
  }

 private:
  friend class KeyboardBacklightColorControllerTest;

  // Displays the |backlight_color| on the keyboard.
  void DisplayBacklightColor(
      personalization_app::mojom::BacklightColor backlight_color);

  // Sets the keyboard backlight color pref for user with |account_id|.
  void SetBacklightColorPref(
      personalization_app::mojom::BacklightColor backlight_color,
      const AccountId& account_id);

  SkColor displayed_color_for_testing_ = SK_ColorTRANSPARENT;

  ScopedSessionObserver scoped_session_observer_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};

  std::unique_ptr<KeyboardBacklightColorNudgeController>
      keyboard_backlight_color_nudge_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_CONTROLLER_H_
