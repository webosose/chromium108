// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/unused_site_permissions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/permissions/features.h"
#include "components/permissions/unused_site_permissions_service.h"
#include "content/public/test/browser_test.h"

class UnusedSitePermissionsServiceBrowserTest : public InProcessBrowserTest {
 public:
  UnusedSitePermissionsServiceBrowserTest() {
    feature_list.InitAndEnableFeature(
        permissions::features::kRecordPermissionExpirationTimestamps);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(UnusedSitePermissionsServiceBrowserTest,
                       TestNavigationUpdatesLastUsedDate) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Create content setting 20 days in the past.
  base::Time now(base::Time::Now());
  base::Time past(now - base::Days(20));
  base::SimpleTestClock clock;
  clock.SetNow(past);
  map->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW,
      {.track_last_visit_for_autoexpiration = true});
  clock.SetNow(now);
  service->UpdateUnusedPermissionsForTesting();
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);

  // Check that the timestamp is initially in the past.
  content_settings::SettingInfo info;
  map->GetWebsiteSetting(url, url, ContentSettingsType::GEOLOCATION, &info);
  ASSERT_FALSE(info.metadata.last_visited.is_null());
  EXPECT_GE(info.metadata.last_visited,
            past - content_settings::GetCoarseTimePrecision());
  EXPECT_LE(info.metadata.last_visited, past);

  // Navigate to |url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the timestamp is updated after a navigation.
  map->GetWebsiteSetting(url, url, ContentSettingsType::GEOLOCATION, &info);
  EXPECT_GE(info.metadata.last_visited,
            now - content_settings::GetCoarseTimePrecision());
  EXPECT_LE(info.metadata.last_visited, now);

  map->SetClockForTesting(base::DefaultClock::GetInstance());
}
