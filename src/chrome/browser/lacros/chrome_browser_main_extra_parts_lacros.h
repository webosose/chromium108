// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#include <memory>

#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"

class ArcIconCache;
class AutomationManagerLacros;
class BrowserServiceLacros;
class ChromeKioskLaunchControllerLacros;
class DeskTemplateClientLacros;
class DeviceLocalAccountExtensionInstallerLacros;
class DriveFsCache;
class DownloadControllerClientLacros;
class ForceInstalledTrackerLacros;
class FullscreenControllerClientLacros;
class LacrosButterBar;
class LacrosExtensionAppsController;
class LacrosExtensionAppsPublisher;
class LacrosFileSystemProvider;
class KioskSessionServiceLacros;
class FieldTrialObserver;
class NetworkChangeManagerBridge;
class QuickAnswersController;
class StandaloneBrowserTestController;
class TabletModePageBehavior;
class UiThroughputRecorderLacros;
class VpnExtensionTrackerLacros;
class WebAuthnRequestRegistrarLacros;

namespace arc {
class ArcIconCacheDelegateProvider;
}  // namespace arc

namespace crosapi {
class SearchControllerLacros;
class TaskManagerLacros;
class WebAppProviderBridgeLacros;
class WebPageInfoProviderLacros;
}  // namespace crosapi

namespace content {
class ScreenOrientationDelegate;
}  // namespace content

// Browser initialization for Lacros.
class ChromeBrowserMainExtraPartsLacros : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsLacros();
  ChromeBrowserMainExtraPartsLacros(const ChromeBrowserMainExtraPartsLacros&) =
      delete;
  ChromeBrowserMainExtraPartsLacros& operator=(
      const ChromeBrowserMainExtraPartsLacros&) = delete;
  ~ChromeBrowserMainExtraPartsLacros() override;

 private:
  // ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PostBrowserStart() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;

  // Receiver and cache of arc icon info updates.
  std::unique_ptr<ArcIconCache> arc_icon_cache_;

  std::unique_ptr<AutomationManagerLacros> automation_manager_;

  // Handles browser action requests from ash-chrome.
  std::unique_ptr<BrowserServiceLacros> browser_service_;

  // Handles requests for desk template data from ash-chrome.
  std::unique_ptr<DeskTemplateClientLacros> desk_template_client_;

  // Handles queries regarding full screen control from ash-chrome.
  std::unique_ptr<FullscreenControllerClientLacros>
      fullscreen_controller_client_;

  // Handles search queries from ash-chrome.
  std::unique_ptr<crosapi::SearchControllerLacros> search_controller_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;

  // Receiver and cache of drive mount point path updates.
  std::unique_ptr<DriveFsCache> drivefs_cache_;

  // Sends lacros download information to ash.
  std::unique_ptr<DownloadControllerClientLacros> download_controller_client_;

  // Sends lacros installation status of force-installed extensions to ash.
  std::unique_ptr<ForceInstalledTrackerLacros> force_installed_tracker_;

  // Receives and handles network change status.
  std::unique_ptr<NetworkChangeManagerBridge> network_change_manager_bridge_;

  // Sends lacros load/unload events of Vpn extensions to ash.
  std::unique_ptr<VpnExtensionTrackerLacros> vpn_extension_tracker_;

  std::unique_ptr<ChromeKioskLaunchControllerLacros>
      chrome_kiosk_launch_controller_;

  // Manages the resources used in the web Kiosk session, and sends window
  // status changes of lacros-chrome to ash when necessary.
  std::unique_ptr<KioskSessionServiceLacros> kiosk_session_service_;

  std::unique_ptr<DeviceLocalAccountExtensionInstallerLacros>
      device_local_account_extension_installer_;

  // Provides ArcIconCache impl.
  std::unique_ptr<arc::ArcIconCacheDelegateProvider>
      arc_icon_cache_delegate_provider_;

  // Handles tab property requests from ash.
  std::unique_ptr<crosapi::WebPageInfoProviderLacros> web_page_info_provider_;

  // Receives web app control commands from ash.
  std::unique_ptr<crosapi::WebAppProviderBridgeLacros> web_app_provider_bridge_;

  // Receives Chrome app (AKA extension app) events from ash.
  std::unique_ptr<LacrosExtensionAppsController> chrome_apps_controller_;

  // Sends Chrome app (AKA extension app) events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> chrome_apps_publisher_;

  // Receives extension events from ash.
  std::unique_ptr<LacrosExtensionAppsController> extensions_controller_;

  // Sends extension events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> extensions_publisher_;

  // A test controller that is registered with the ash-chrome's test controller
  // service over crosapi to let tests running in ash-chrome control this Lacros
  // instance. It is only instantiated in Linux builds AND only when Ash's test
  // controller is available (practically, just test binaries), so this will
  // remain null in production builds.
  std::unique_ptr<StandaloneBrowserTestController>
      standalone_browser_test_controller_;

  // Receiver of field trial updates.
  std::unique_ptr<FieldTrialObserver> field_trial_observer_;

  // Shows a butter bar on the first window.
  std::unique_ptr<LacrosButterBar> butter_bar_;

  // Receives orientation lock data.
  std::unique_ptr<content::ScreenOrientationDelegate>
      screen_orientation_delegate_;

  // Handles WebAuthn request id generation.
  std::unique_ptr<WebAuthnRequestRegistrarLacros>
      webauthn_request_registrar_lacros_;

  // Handles Quick answers requests from the Lacros browser.
  std::unique_ptr<QuickAnswersController> quick_answers_controller_;

  // Updates Blink preferences on tablet mode state change.
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;

  // Forwards file system provider events to extensions.
  std::unique_ptr<LacrosFileSystemProvider> file_system_provider_;

  // Records UI metrics such as dropped frame percentage.
  std::unique_ptr<UiThroughputRecorderLacros> ui_throughput_recorder_;

  // Controls sync-related Crosapi clients.
  SyncCrosapiManagerLacros sync_crosapi_manager_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
