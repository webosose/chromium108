// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class WebAppCommandSchedulerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());
  }

  FakeWebAppProvider* provider() { return provider_; }

 private:
  FakeWebAppProvider* provider_;
};

TEST_F(WebAppCommandSchedulerTest, FetchManifestAndInstall) {
  EXPECT_FALSE(provider()->is_registry_ready());
  provider()->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/true, base::DoNothing(),
      base::DoNothing(), /*use_fallback=*/true);

  provider()->StartWithSubsystems();
  EXPECT_EQ(provider()->command_manager().GetCommandCountForTesting(), 0u);

  base::RunLoop run_loop;
  provider()->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(provider()->command_manager().GetCommandCountForTesting(), 1u);
  base::Value::Dict log =
      provider()->command_manager().ToDebugValue().TakeDict();
  base::Value::List* command_queue = log.FindList("command_queue");

  EXPECT_EQ(command_queue->size(), 1u);
  EXPECT_EQ(*command_queue->front().GetDict().FindDict("value")->FindString(
                "command_name"),
            "FetchManifestAndInstallCommand");
}

}  // namespace
}  // namespace web_app
