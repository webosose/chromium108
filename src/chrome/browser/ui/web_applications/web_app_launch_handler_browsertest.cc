// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/manifest_update_task.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/embedder_support/switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace web_app {

using ClientMode = LaunchHandler::ClientMode;

class WebAppLaunchHandlerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppLaunchHandlerBrowserTest() = default;
  ~WebAppLaunchHandlerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  AppId InstallTestWebApp(const char* test_file_path,
                          bool await_metric = true) {
    BrowserWaiter browser_waiter;

    page_load_metrics::PageLoadMetricsTestWaiter metrics_waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    if (await_metric) {
      metrics_waiter.AddWebFeatureExpectation(
          blink::mojom::WebFeature::kWebAppManifestLaunchHandler);
    }

    AppId app_id = InstallWebAppFromPage(
        browser(), embedded_test_server()->GetURL(test_file_path));

    if (await_metric)
      metrics_waiter.Wait();

    // Installing a web app will pop it out to a new window.
    // Close this to avoid it interfering with test steps.
    Browser* app_browser = browser_waiter.AwaitAdded();
    chrome::CloseWindow(app_browser);
    browser_waiter.AwaitRemoved();

    return app_id;
  }

  const WebApp* GetWebApp(const AppId& app_id) {
    return WebAppProvider::GetForTest(profile())->registrar().GetAppById(
        app_id);
  }

  absl::optional<LaunchHandler> GetLaunchHandler(const AppId& app_id) {
    return GetWebApp(app_id)->launch_handler();
  }

  std::string AwaitNextLaunchParamsTargetUrl(Browser* browser) {
    const char* script = R"(
      new Promise(resolve => {
        window.launchQueue.setConsumer(resolve);
      }).then(params => params.targetURL);
    )";
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractString();
  }

  bool SetUpNextLaunchParamsTargetUrlPromise(Browser* browser) {
    const char* script = R"(
        window.nextLaunchParamsTargetURLPromise = new Promise(resolve => {
          window.launchQueue.setConsumer(resolve);
        }).then(params => params.targetURL);
        true;
      )";
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractBool();
  }

  std::string AwaitNextLaunchParamsTargetUrlPromise(Browser* browser) {
    return EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                  "window.nextLaunchParamsTargetURLPromise")
        .ExtractString();
  }

  void ExpectNavigateExistingBehaviour(const AppId& app_id,
                                       const GURL& start_url) {
    EXPECT_EQ(GetLaunchHandler(app_id),
              (LaunchHandler{ClientMode::kNavigateExisting}));

    Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
    content::WebContents* web_contents =
        browser_1->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url.spec());

    // Navigate window away from start_url to check that the next launch navs to
    // start_url again.
    GURL alt_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    NavigateToURLAndWait(browser_1, alt_url);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), alt_url);

    Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
    EXPECT_EQ(browser_1, browser_2);
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url.spec());
  }

  void ExpectFocusExistingBehaviour(const AppId& app_id,
                                    const GURL& start_url) {
    EXPECT_EQ(GetLaunchHandler(app_id),
              (LaunchHandler{ClientMode::kFocusExisting}));

    Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
    content::WebContents* web_contents =
        browser_1->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
    EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url.spec());

    // Navigate window away from start_url to an in scope URL, check that the
    // next launch doesn't navigate to start_url.
    {
      GURL in_scope_url =
          embedded_test_server()->GetURL("/web_apps/basic.html");
      NavigateToURLAndWait(browser_1, in_scope_url);
      EXPECT_EQ(web_contents->GetLastCommittedURL(), in_scope_url);

      ASSERT_TRUE(SetUpNextLaunchParamsTargetUrlPromise(browser_1));
      Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
      EXPECT_EQ(browser_1, browser_2);
      EXPECT_EQ(AwaitNextLaunchParamsTargetUrlPromise(browser_2),
                start_url.spec());
      EXPECT_EQ(web_contents->GetLastCommittedURL(), in_scope_url);
    }

    // Navigate window away from start_url to an out of scope URL, check that
    // the next launch does navigate to start_url.
    {
      GURL out_of_scope_url = embedded_test_server()->GetURL("/empty.html");
      NavigateToURLAndWait(browser_1, out_of_scope_url);
      EXPECT_EQ(web_contents->GetLastCommittedURL(), out_of_scope_url);

      Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
      EXPECT_EQ(browser_1, browser_2);
      EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url.spec());
      EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);
    }

    // Trigger launch during navigation, check that the navigation gets
    // cancelled.
    {
      ASSERT_TRUE(EvalJs(web_contents, "window.thisIsTheSamePage = true")
                      .ExtractBool());

      GURL hanging_url = embedded_test_server()->GetURL("/hang");
      NavigateParams params(browser_1, hanging_url, ui::PAGE_TRANSITION_LINK);
      Navigate(&params);
      EXPECT_EQ(web_contents->GetVisibleURL(), hanging_url);
      EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

      ASSERT_TRUE(SetUpNextLaunchParamsTargetUrlPromise(browser_1));
      Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
      EXPECT_EQ(browser_1, browser_2);
      EXPECT_EQ(AwaitNextLaunchParamsTargetUrlPromise(browser_2),
                start_url.spec());
      EXPECT_EQ(web_contents->GetVisibleURL(), start_url);
      EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

      // Check that we never left the current page.
      EXPECT_TRUE(
          EvalJs(web_contents, "window.thisIsTheSamePage").ExtractBool());
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kWebAppEnableLaunchHandler};
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeEmpty) {
  AppId app_id =
      InstallTestWebApp("/web_apps/basic.html", /*await_metric=*/false);
  EXPECT_EQ(GetLaunchHandler(app_id), absl::nullopt);

  Browser* browser_1 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeAuto) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?launch_handler_client_mode_auto.json");
  EXPECT_EQ(GetLaunchHandler(app_id), (LaunchHandler{ClientMode::kAuto}));

  std::string start_url = GetWebApp(app_id)->start_url().spec();

  Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url);

  Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url);

  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, ClientModeNavigateNew) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_new.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateNew}));

  std::string start_url = GetWebApp(app_id)->start_url().spec();

  Browser* browser_1 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_1), start_url);

  Browser* browser_2 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(AwaitNextLaunchParamsTargetUrl(browser_2), start_url);

  EXPECT_NE(browser_1, browser_2);
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeNavigateExisting) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");
  ExpectNavigateExistingBehaviour(
      app_id, embedded_test_server()->GetURL("/web_apps/basic.html?"
                                             "client_mode=navigate-existing"));
}

// TODO(crbug.com/1308334): Fix flakiness.
IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       DISABLED_ClientModeExistingClientRetain) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  ExpectFocusExistingBehaviour(
      app_id, embedded_test_server()->GetURL("/web_apps/basic.html?"
                                             "client_mode=focus-existing"));
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeFocusExistingMultipleLaunches) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kFocusExisting}));

  // Launch the app three times in quick succession.
  Browser* browser_1 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_3 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(browser_1, browser_2);
  EXPECT_EQ(browser_2, browser_3);

  // Check that all 3 LaunchParams got enqueued.
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?client_mode=focus-existing");
  const char* script = R"(
      new Promise(resolve => {
        let remaining = 3;
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
          if (--remaining == 0) {
            resolve(targetURLs.join('|'));
          }
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(),
            base::StrCat({start_url.spec(), "|", start_url.spec(), "|",
                          start_url.spec()}));
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       ClientModeNavigateExistingMultipleLaunches) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_existing.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateExisting}));

  // Launch the app three times in quick succession.
  Browser* browser_1 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_2 = LaunchWebAppBrowser(profile(), app_id);
  Browser* browser_3 = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(browser_1, browser_2);
  EXPECT_EQ(browser_2, browser_3);

  // Check that only the last LaunchParams made it through.
  content::WebContents* web_contents =
      browser_1->tab_strip_model()->GetActiveWebContents();
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/basic.html?client_mode=navigate-existing");
  const char* script = R"(
      new Promise(resolve => {
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
          // Wait a tick to let any additional erroneous launchParams get added.
          requestAnimationFrame(() => {
            resolve(targetURLs.join('|'));
          });
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(), start_url.spec());
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest,
                       LaunchNavigationInterruptedByOutOfScopeNavigation) {
  AppId app_id = InstallTestWebApp(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_navigate_new.json");
  EXPECT_EQ(GetLaunchHandler(app_id),
            (LaunchHandler{ClientMode::kNavigateNew}));

  // Launch the web app and immediately navigate it out of scope during its
  // initial navigation.
  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  GURL out_of_scope_url = embedded_test_server()->GetURL("/empty.html");
  NavigateToURLAndWait(app_browser, out_of_scope_url);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), out_of_scope_url);

  // Check that the launch params are not enqueued in the out of scope document.
  const char* script = R"(
      new Promise(resolve => {
        let targetURLs = [];
        window.launchQueue.setConsumer(launchParams => {
          targetURLs.push(launchParams.targetURL);
        });
        // Wait a tick to let any erroneous launch params get added.
        requestAnimationFrame(() => {
          resolve(targetURLs.join('|'));
        });
      });
    )";
  EXPECT_EQ(EvalJs(web_contents, script).ExtractString(), "");
}

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerBrowserTest, GlobalLaunchQueue) {
  AppId app_id =
      InstallTestWebApp("/web_apps/basic.html", /*await_metric=*/false);

  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.launchQueue").ExtractBool());
  EXPECT_TRUE(EvalJs(web_contents, "!!window.LaunchParams").ExtractBool());
}

class WebAppLaunchHandlerDisabledBrowserTest : public InProcessBrowserTest {
 public:
  WebAppLaunchHandlerDisabledBrowserTest() {
    feature_list_.InitWithFeatures({},
                                   {blink::features::kWebAppEnableLaunchHandler,
                                    blink::features::kFileHandlingAPI});
  }
  ~WebAppLaunchHandlerDisabledBrowserTest() override = default;

  Profile* profile() { return browser()->profile(); }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerDisabledBrowserTest, NoLaunchQueue) {
  AppId app_id = InstallWebAppFromPage(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  Browser* app_browser = LaunchWebAppBrowser(profile(), app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(EvalJs(web_contents, "!!window.LaunchQueue").ExtractBool());
  EXPECT_FALSE(EvalJs(web_contents, "!!window.launchQueue").ExtractBool());
  EXPECT_FALSE(EvalJs(web_contents, "!!window.LaunchParams").ExtractBool());
}

class WebAppLaunchHandlerOriginTrialBrowserTest : public InProcessBrowserTest {
 public:
  WebAppLaunchHandlerOriginTrialBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kWebAppEnableLaunchHandler);
  }
  ~WebAppLaunchHandlerOriginTrialBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Using the test public key from docs/origin_trials_integration.md#Testing.
    command_line->AppendSwitchASCII(
        embedder_support::kOriginTrialPublicKey,
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};
namespace {

// InstallableManager requires https or localhost to load the manifest. Go with
// localhost to avoid having to set up cert servers.
constexpr char kTestWebAppUrl[] = "http://127.0.0.1:8000/";
constexpr char kTestWebAppHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
constexpr char kTestWebAppBody[] = R"(
  <!DOCTYPE html>
  <head>
    <link rel="manifest" href="manifest.webmanifest">
    <meta http-equiv="origin-trial" content="$1">
  </head>
)";

constexpr char kTestIconUrl[] = "http://127.0.0.1:8000/icon.png";
constexpr char kTestManifestUrl[] =
    "http://127.0.0.1:8000/manifest.webmanifest";
constexpr char kTestManifestHeaders[] =
    "HTTP/1.1 200 OK\nContent-Type: application/json; charset=utf-8\n";
constexpr char kTestManifestBody[] = R"({
  "name": "Test app",
  "display": "standalone",
  "start_url": "/",
  "scope": "/",
  "icons": [{
    "src": "icon.png",
    "sizes": "192x192",
    "type": "image/png"
  }],
  "launch_handler": {
    "client_mode": "focus-existing"
  }
})";

// Generated from script:
// $ tools/origin_trials/generate_token.py http://127.0.0.1:8000 "Launch
// Handler" --expire-timestamp=2000000000
constexpr char kOriginTrialToken[] =
    "A12ynArMVnf0OepZoKB23txoJ/"
    "jicU25In+"
    "UseVdaSziSYtPMfobhyEhFdVasQ90uo4LMf2G6AIyRFxALB4oQgEAAABWeyJvcmlnaW4iOiAia"
    "HR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiTGF1bmNoIEhhbmRsZXIiLCAiZXh"
    "waXJ5IjogMjAwMDAwMDAwMH0=";

}  // namespace

IN_PROC_BROWSER_TEST_F(WebAppLaunchHandlerOriginTrialBrowserTest, OriginTrial) {
  ManifestUpdateTask::BypassWindowCloseWaitingForTesting() = true;

  bool serve_token = true;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&serve_token](
          content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.spec() == kTestWebAppUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestWebAppHeaders,
              base::ReplaceStringPlaceholders(
                  kTestWebAppBody, {serve_token ? kOriginTrialToken : ""},
                  nullptr),
              params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestManifestUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              kTestManifestHeaders, kTestManifestBody, params->client.get());
          return true;
        }
        if (params->url_request.url.spec() == kTestIconUrl) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/basic-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  // Install web app with origin trial token.
  AppId app_id = InstallWebAppFromPage(browser(), GURL(kTestWebAppUrl));

  // Origin trial should grant the app access.
  WebAppProvider& provider = *WebAppProvider::GetForTest(browser()->profile());
  EXPECT_EQ(provider.registrar().GetAppById(app_id)->launch_handler(),
            (LaunchHandler{ClientMode::kFocusExisting}));

  // Open the page again with the token missing.
  {
    UpdateAwaiter update_awaiter(provider.install_manager());

    serve_token = false;
    NavigateToURLAndWait(browser(), GURL(kTestWebAppUrl));

    update_awaiter.AwaitUpdate();
  }

  // The app should update to no longer have launch_handler defined without the
  // origin trial.
  EXPECT_EQ(provider.registrar().GetAppById(app_id)->launch_handler(),
            absl::nullopt);
}

}  // namespace web_app
