// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/guid.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "content/public/test/browser_test.h"

namespace extensions {

class WmDesksPrivateApiTest : public ExtensionApiTest {
 public:
  WmDesksPrivateApiTest() {
    scoped_feature_list.InitWithFeatures(
        /*enabled_features=*/{ash::features::kEnableSavedDesks,
                              ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }

  ~WmDesksPrivateApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

// General API test for desk API.
// API test is flaky when involves animation. For APIs involving animation use
// browser test instead.
// TODO(crbug.com/1370233): Re-enable this test
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, DISABLED_WmDesksPrivateApiTest) {
  // This loads and runs an extension from
  // chrome/test/data/extensions/api_test/wm_desks_private.
  ASSERT_TRUE(RunExtensionTest("wm_desks_private"));
}

// Tests launch and close a desk.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, LaunchAndCloseDeskTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          launch_desk_function.get(), R"([{"deskName":"test"}])", new_browser);
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() + R"(", { "combineDesks": false }])",
      new_browser);

  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
  // Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }
}

// Tests launch and list all desk.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, ListDesksTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());
  ash::DeskSwitchAnimationWaiter waiter;
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  // Asserts no error.
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          launch_desk_function.get(), R"([{"deskName":"test"}])", new_browser);
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    waiter.Wait();
  }

  // List All Desks.
  auto list_desks_function =
      base::MakeRefCounted<WmDesksPrivateGetAllDesksFunction>();
  auto all_desks =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          list_desks_function.get(), "[]", new_browser);
  EXPECT_TRUE(all_desks->is_list());
  EXPECT_EQ(2u, all_desks->GetList().size());
}

// Tests switch to different desk show trigger animation.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchToDifferentDeskTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());
  base::HistogramTester histogram_tester;
  // Get the active desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function.get(), "[]", new_browser);
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  // Asserts no error.
  auto desk_id_1 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          launch_desk_function.get(), R"([{"deskName":"test"}])", new_browser);
  EXPECT_TRUE(desk_id_1->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id_1->GetString()).is_valid());
  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter switch_waiter;
  // Switches to the previous desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();

  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      new_browser);

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    switch_waiter.Wait();
  }

  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id_2 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function_.get(), "[]", new_browser);
  EXPECT_TRUE(desk_id_2->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_2->GetString());
  histogram_tester.ExpectBucketCount("Ash.DeskApi.SwitchDesk.Result", 1, 1);
}

// Tests switch to current desk should skip animation.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchToCurrentDeskTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  // Get the desk desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function.get(), "[]", new_browser);
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Switches to the current desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();
  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      new_browser);

  // Get the current desk.
  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  auto desk_id_1 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function_.get(), "[]", new_browser);
  EXPECT_TRUE(desk_id_1->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_1->GetString());
}

// Tests launch desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest,
                       LaunchDeskWhenMaxNumberExceedTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  // Max number of desks allowed is 8.
  const int kMaxDeskIndex = 7;
  for (int i = 0; i < kMaxDeskIndex; i++) {
    ash::DesksController::Get()->NewDesk(ash::DesksCreationRemovalSource::kApi);
  }

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto error = extension_function_test_utils::RunFunctionAndReturnError(
      launch_desk_function.get(), R"([{"deskName":"test"}])", new_browser);
  EXPECT_EQ(error, "The maximum number of desks is already open.");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 0, 1);
}

// Tests remove desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, RemoveDeskWithInvalidIdTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  auto error = extension_function_test_utils::RunFunctionAndReturnError(
      remove_desk_function.get(), R"(["invalid-id"])", new_browser);

  EXPECT_EQ(error, "The desk identifier is not valid.");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 0, 1);
}

// Tests switch desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchDeskWithInvalidIdTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  base::HistogramTester histogram_tester;
  // Switches to the current desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();
  auto error = extension_function_test_utils::RunFunctionAndReturnError(
      switch_desk_function.get(), R"(["invalid-id"])", new_browser);

  EXPECT_EQ(error, "The desk identifier is not valid.");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.SwitchDesk.Result", 0, 1);
}

// Tests set all desks with invalid ID.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest,
                       SetAllDesksWindowWithInvalidIdTest) {
  Browser* new_browser = CreateBrowser(browser()->profile());

  base::HistogramTester histogram_tester;
  // Switches to the current desk.
  auto all_desk_function =
      base::MakeRefCounted<WmDesksPrivateSetWindowPropertiesFunction>();
  auto error = extension_function_test_utils::RunFunctionAndReturnError(
      all_desk_function.get(), R"([123,{"allDesks":true}])", new_browser);

  EXPECT_EQ(error, "The window cannot be found.");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.AllDesk.Result", 0, 1);
}

}  // namespace extensions
