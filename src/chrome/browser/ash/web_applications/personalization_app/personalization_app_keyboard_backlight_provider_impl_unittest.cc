// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
const AccountId account_id =
    AccountId::FromUserEmailGaiaId(kFakeTestEmail, kFakeTestEmail);

class TestKeyboardBacklightObserver
    : public ash::personalization_app::mojom::KeyboardBacklightObserver {
 public:
  void OnBacklightColorChanged(mojom::BacklightColor backlight_color) override {
    backlight_color_ = backlight_color;
  }

  void OnWallpaperColorChanged(SkColor wallpaper_color) override {
    wallpaper_color_ = wallpaper_color;
  }

  mojo::PendingRemote<
      ash::personalization_app::mojom::KeyboardBacklightObserver>
  pending_remote() {
    if (keyboard_backlight_observer_receiver_.is_bound()) {
      keyboard_backlight_observer_receiver_.reset();
    }

    return keyboard_backlight_observer_receiver_.BindNewPipeAndPassRemote();
  }

  mojom::BacklightColor backlight_color() {
    keyboard_backlight_observer_receiver_.FlushForTesting();
    return backlight_color_;
  }

  SkColor wallpaper_color() {
    keyboard_backlight_observer_receiver_.FlushForTesting();
    return wallpaper_color_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::KeyboardBacklightObserver>
      keyboard_backlight_observer_receiver_{this};

  mojom::BacklightColor backlight_color_ = mojom::BacklightColor::kWallpaper;
  SkColor wallpaper_color_ = SK_ColorTRANSPARENT;
};

}  // namespace

class PersonalizationAppKeyboardBacklightProviderImplTest
    : public ChromeAshTestBase {
 public:
  PersonalizationAppKeyboardBacklightProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures({ash::features::kRgbKeyboard}, {});
  }
  PersonalizationAppKeyboardBacklightProviderImplTest(
      const PersonalizationAppKeyboardBacklightProviderImplTest&) = delete;
  PersonalizationAppKeyboardBacklightProviderImplTest& operator=(
      const PersonalizationAppKeyboardBacklightProviderImplTest&) = delete;
  ~PersonalizationAppKeyboardBacklightProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    ash::FakeChromeUserManager* user_manager =
        static_cast<ash::FakeChromeUserManager*>(
            user_manager::UserManager::Get());
    user_manager->AddUser(account_id);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    keyboard_backlight_provider_ =
        std::make_unique<PersonalizationAppKeyboardBacklightProviderImpl>(
            &web_ui_);
    keyboard_backlight_provider_->BindInterface(
        keyboard_backlight_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    keyboard_backlight_provider_.reset();
    ChromeAshTestBase::TearDown();
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::KeyboardBacklightProvider>*
  keyboard_backlight_provider_remote() {
    return &keyboard_backlight_provider_remote_;
  }

  PersonalizationAppKeyboardBacklightProviderImpl*
  keyboard_backlight_provider() {
    return keyboard_backlight_provider_.get();
  }

  void SetKeyboardBacklightObserver() {
    keyboard_backlight_provider_remote_->SetKeyboardBacklightObserver(
        test_keyboard_backlight_observer_.pending_remote());
  }

  mojom::BacklightColor ObservedBacklightColor() {
    keyboard_backlight_provider_remote_.FlushForTesting();
    return test_keyboard_backlight_observer_.backlight_color();
  }

  SkColor ObservedWallpaperColor() {
    keyboard_backlight_provider_remote_.FlushForTesting();
    return test_keyboard_backlight_observer_.wallpaper_color();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::KeyboardBacklightProvider>
      keyboard_backlight_provider_remote_;
  std::unique_ptr<PersonalizationAppKeyboardBacklightProviderImpl>
      keyboard_backlight_provider_;
  TestKeyboardBacklightObserver test_keyboard_backlight_observer_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PersonalizationAppKeyboardBacklightProviderImplTest, SetBacklightColor) {
  SimulateUserLogin(account_id);
  SetKeyboardBacklightObserver();
  keyboard_backlight_provider_remote()->FlushForTesting();
  keyboard_backlight_provider()->SetBacklightColor(
      mojom::BacklightColor::kBlue);

  // Verify JS side is notified.
  EXPECT_EQ(mojom::BacklightColor::kBlue, ObservedBacklightColor());
  histogram_tester().ExpectBucketCount(
      kPersonalizationKeyboardBacklightColorHistogramName,
      mojom::BacklightColor::kBlue, 1);
}

TEST_F(PersonalizationAppKeyboardBacklightProviderImplTest,
       ObserveWallpaperColor) {
  SimulateUserLogin(account_id);
  SetKeyboardBacklightObserver();
  keyboard_backlight_provider_remote()->FlushForTesting();
  keyboard_backlight_provider()->OnWallpaperColorsChanged();

  // Verify JS side is notified.
  EXPECT_EQ(SK_ColorTRANSPARENT, ObservedWallpaperColor());
}

}  // namespace ash::personalization_app
