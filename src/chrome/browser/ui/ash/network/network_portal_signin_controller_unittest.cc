// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

constexpr char kTestPortalUrl[] = "http://www.gstatic.com/generate_204";

class TestSigninController : public NetworkPortalSigninController {
 public:
  TestSigninController() = default;
  TestSigninController(const TestSigninController&) = delete;
  TestSigninController& operator=(const TestSigninController&) = delete;
  ~TestSigninController() override = default;

  // NetworkPortalSigninController
  void ShowDialog(Profile* profile, const GURL& url) override {
    profile_ = profile;
    dialog_url_ = url.spec();
  }
  void ShowSingletonTab(Profile* profile, const GURL& url) override {
    profile_ = profile;
    singleton_tab_url_ = url.spec();
  }
  void ShowTab(Profile* profile, const GURL& url) override {
    profile_ = profile;
    tab_url_ = url.spec();
  }

  Profile* profile() const { return profile_; }
  const std::string& dialog_url() const { return dialog_url_; }
  const std::string& singleton_tab_url() const { return singleton_tab_url_; }
  const std::string& tab_url() const { return tab_url_; }

 private:
  Profile* profile_ = nullptr;
  std::string dialog_url_;
  std::string singleton_tab_url_;
  std::string tab_url_;
};

}  // namespace

class NetworkPortalSigninControllerTest : public testing::Test {
 public:
  NetworkPortalSigninControllerTest() {
    feature_list_.InitAndDisableFeature({features::kCaptivePortalUI2022});
  }
  NetworkPortalSigninControllerTest(const NetworkPortalSigninControllerTest&) =
      delete;
  NetworkPortalSigninControllerTest& operator=(
      const NetworkPortalSigninControllerTest&) = delete;
  ~NetworkPortalSigninControllerTest() override = default;

  void SetUp() override {
    network_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    controller_ = std::make_unique<TestSigninController>();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    user_manager_enabler_.reset();
    network_helper_.reset();
  }

 protected:
  void SimulateLogin() {
    const AccountId test_account_id(
        AccountId::FromUserEmail("test_user@gmail.com"));
    user_manager_ = new FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_));

    test_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(test_profile_manager_->SetUp());
    test_profile_manager_->CreateTestingProfile(test_account_id.GetUserEmail());

    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);
  }

  PrefService* GetPrefs() {
    DCHECK(test_profile_manager_);
    PrefService* prefs = test_profile_manager_->profile_manager()
                             ->GetActiveUserProfile()
                             ->GetPrefs();
    DCHECK(prefs);
    return prefs;
  }

  const NetworkState& GetDefaultNetwork() {
    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    DCHECK(network);
    return *network;
  }

  std::string SetProbeUrl(const std::string& url) {
    std::string expected_url;
    if (!url.empty()) {
      std::string default_path = GetDefaultNetwork().path();
      ShillServiceClient::Get()->SetProperty(
          dbus::ObjectPath(default_path), shill::kProbeUrlProperty,
          base::Value(url), base::DoNothing(), base::DoNothing());
      base::RunLoop().RunUntilIdle();
      expected_url = url;
    } else {
      expected_url = captive_portal::CaptivePortalDetector::kDefaultURL;
    }
    return expected_url;
  }

  void SetNetworkProxy() {
    GetPrefs()->SetBoolean(::proxy_config::prefs::kUseSharedProxies, true);
    proxy_config::SetProxyConfigForNetwork(
        ProxyConfigDictionary(ProxyConfigDictionary::CreateAutoDetect()),
        GetDefaultNetwork());
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkProxyDirect() {
    GetPrefs()->SetBoolean(::proxy_config::prefs::kUseSharedProxies, true);
    proxy_config::SetProxyConfigForNetwork(
        ProxyConfigDictionary(ProxyConfigDictionary::CreateDirect()),
        GetDefaultNetwork());
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_helper_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  FakeChromeUserManager* user_manager_;
  std::unique_ptr<TestingProfileManager> test_profile_manager_;
  std::unique_ptr<TestSigninController> controller_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NetworkPortalSigninControllerTest, LoginScreen) {
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, KioskMode) {
  SimulateLogin();
  const user_manager::User* user = user_manager_->AddKioskAppUser(
      AccountId::FromUserEmail("fake_user@test"));
  user_manager_->LoginUser(user->GetAccountId());

  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyTrue) {
  SimulateLogin();
  // kCaptivePortalAuthenticationIgnoresProxy defaults to true
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, AuthenticationIgnoresProxyFalse) {
  SimulateLogin();
  GetPrefs()->SetBoolean(prefs::kCaptivePortalAuthenticationIgnoresProxy,
                         false);
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->singleton_tab_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest, ProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->dialog_url(), expected_url);
}

TEST_F(NetworkPortalSigninControllerTest, NoProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  controller_->ShowSignin();
  EXPECT_EQ(controller_->dialog_url(), expected_url);
}

class NetworkPortalSigninControllerTest2022Update
    : public NetworkPortalSigninControllerTest {
 public:
  NetworkPortalSigninControllerTest2022Update() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(features::kCaptivePortalUI2022);
  }
};

TEST_F(NetworkPortalSigninControllerTest2022Update, LoginScreen) {
  controller_->ShowSignin();
  EXPECT_FALSE(controller_->dialog_url().empty());
}

TEST_F(NetworkPortalSigninControllerTest2022Update, NoProxy) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest2022Update, ProxyDirect) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxyDirect();
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest2022Update,
       AuthenticationIgnoresProxyTrue) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  // kCaptivePortalAuthenticationIgnoresProxy defaults to true
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest2022Update,
       AuthenticationIgnoresProxyFalse) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  GetPrefs()->SetBoolean(prefs::kCaptivePortalAuthenticationIgnoresProxy,
                         false);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_FALSE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest2022Update,
       AuthenticationIgnoresProxyFalseOTRDisabled) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  SetNetworkProxy();
  IncognitoModePrefs::SetAvailability(
      GetPrefs(), IncognitoModePrefs::Availability::kDisabled);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->dialog_url(), expected_url);
}

TEST_F(NetworkPortalSigninControllerTest2022Update, ProxyPref) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  base::Value::Dict proxy_config;
  proxy_config.Set("mode", ProxyPrefs::kPacScriptProxyModeName);
  proxy_config.Set("pac_url", "http://proxy");
  GetPrefs()->SetDict(::proxy_config::prefs::kProxy, std::move(proxy_config));
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

TEST_F(NetworkPortalSigninControllerTest2022Update, NoProbeUrl) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(std::string());
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
}

TEST_F(NetworkPortalSigninControllerTest2022Update, IsNewOTRProfile) {
  SimulateLogin();
  std::string expected_url = SetProbeUrl(kTestPortalUrl);
  controller_->ShowSignin();
  EXPECT_EQ(controller_->tab_url(), expected_url);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Profile* default_otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_NE(profile, default_otr_profile);
  EXPECT_NE(controller_->profile(), profile);
  EXPECT_NE(controller_->profile(), default_otr_profile);
  EXPECT_TRUE(controller_->profile()->IsOffTheRecord());
}

}  // namespace ash
