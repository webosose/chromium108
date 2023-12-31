// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/system_page.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakeNetworkGuidInfoList, fakeSystemInfo, fakeSystemInfoWithoutBattery, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {SystemPageElement} from 'chrome://diagnostics/system_page.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {isVisible} from '../../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

/**
 * @param {Array<?T>} cards
 * @template T
 * @throws {!Error}
 */
function assertRunTestButtonsDisabled(cards) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertTrue(runTestsButton.disabled);
  });
}

/**
 * @param {Array<?T>} cards
 * @template T
 * @throws {!Error}
 */
function assertRunTestButtonsEnabled(cards) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertFalse(runTestsButton.disabled);
  });
}

export function systemPageTestSuite() {
  /** @type {?SystemPageElement} */
  let page = null;

  /** @type {?FakeSystemDataProvider} */
  let systemDataProvider = null;

  /** @type {?FakeNetworkHealthProvider} */
  let networkHealthProvider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    systemDataProvider = new FakeSystemDataProvider();
    networkHealthProvider = new FakeNetworkHealthProvider();

    setSystemDataProviderForTesting(systemDataProvider);
    setNetworkHealthProviderForTesting(networkHealthProvider);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    page.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  /**
   * @param {!SystemInfo} systemInfo
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatus
   * @param {!Array<!BatteryHealth>} batteryHealth
   * @param {!BatteryInfo} batteryInfo
   * @param {!Array<!CpuUsage>} cpuUsage
   * @param {!Array<!MemoryUsage>} memoryUsage
   */
  function initializeSystemPage(
      systemInfo, batteryChargeStatus, batteryHealth, batteryInfo, cpuUsage,
      memoryUsage) {
    assertFalse(!!page);

    // Initialize the fake data.
    systemDataProvider.setFakeSystemInfo(systemInfo);
    systemDataProvider.setFakeBatteryChargeStatus(batteryChargeStatus);
    systemDataProvider.setFakeBatteryHealth(batteryHealth);
    systemDataProvider.setFakeBatteryInfo(batteryInfo);
    systemDataProvider.setFakeCpuUsage(cpuUsage);
    systemDataProvider.setFakeMemoryUsage(memoryUsage);

    networkHealthProvider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    networkHealthProvider.setFakeNetworkState(
        'ethernetGuid', [fakeEthernetNetwork]);
    networkHealthProvider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    networkHealthProvider.setFakeNetworkState(
        'cellularGuid', [fakeCellularNetwork]);

    page =
        /** @type {!SystemPageElement} */ (
            document.createElement('system-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    page.isNetworkingEnabled = false;
    return flushTasks();
  }

  /**
   * Get the session log button.
   * @return {!CrButtonElement}
   */
  function getSessionLogButton() {
    return /** @type {!CrButtonElement} */ (
        page.shadowRoot.querySelector('.session-log-button'));
  }

  /**
   * Clicks the session log button.
   * @return {!Promise}
   */
  function clickSessionLogButton() {
    getSessionLogButton().click();
    return flushTasks();
  }

  /**
   * Returns whether the toast is visible or not.
   * @return {boolean}
   */
  function isToastVisible() {
    return page.shadowRoot.querySelector('cr-toast').open;
  }

  /**
   * @param {boolean} isLoggedIn
   * @suppress {visibility} // access private member
   * @return {!Promise}
   */
  function changeLoggedInState(isLoggedIn) {
    page.isLoggedIn_ = isLoggedIn;
    return flushTasks();
  }

  /**
   * Get the caution banner.
   * @return {!HTMLElement}
   */
  function getCautionBanner() {
    return /** @type {!HTMLElement} */ (
        page.shadowRoot.querySelector('#banner'));
  }

  test('LandingPageLoaded', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          // Verify the overview card is in the page.
          const overview = page.shadowRoot.querySelector('#overviewCard');
          assertTrue(!!overview);

          // Verify the memory card is in the page.
          const memory = page.shadowRoot.querySelector('#memoryCard');
          assertTrue(!!memory);

          // Verify the CPU card is in the page.
          const cpu = page.shadowRoot.querySelector('#cpuCard');
          assertTrue(!!cpu);

          // Verify the battery status card is in the page.
          const batteryStatus =
              page.shadowRoot.querySelector('#batteryStatusCard');
          assertTrue(!!batteryStatus);

          // Verify the session log button is in the page.
          const sessionLog =
              page.shadowRoot.querySelector('.session-log-button');
          assertTrue(!!sessionLog);
        });
  });

  test('BatteryStatusCardHiddenIfNotSupported', () => {
    return initializeSystemPage(
               fakeSystemInfoWithoutBattery, fakeBatteryChargeStatus,
               fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsage)
        .then(() => {
          // Verify the battery status card is not in the page.
          const batteryStatus =
              page.shadowRoot.querySelector('#batteryStatusCard');
          assertFalse(!!batteryStatus);
        });
  });

  test('AllRunTestsButtonsDisabledWhileRunning', () => {
    let cards = null;
    let memoryRoutinesSection = null;
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          const batteryStatusCard =
              page.shadowRoot.querySelector('battery-status-card');
          const cpuCard = page.shadowRoot.querySelector('cpu-card');
          const memoryCard = page.shadowRoot.querySelector('memory-card');
          cards = [batteryStatusCard, cpuCard, memoryCard];

          memoryRoutinesSection = dx_utils.getRoutineSection(memoryCard);
          memoryRoutinesSection.testSuiteStatus = TestSuiteStatus.RUNNING;
          return flushTasks();
        })
        .then(() => {
          assertRunTestButtonsDisabled(cards);
          memoryRoutinesSection.testSuiteStatus = TestSuiteStatus.NOT_RUNNING;
          return flushTasks();
        })
        .then(() => assertRunTestButtonsEnabled(cards));
  });

  test('SaveSessionLogDisabledWhenPendingResult', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          assertFalse(getSessionLogButton().disabled);
          DiagnosticsBrowserProxy.setSuccess(true);

          getSessionLogButton().click();
          assertTrue(getSessionLogButton().disabled);
          return flushTasks();
        })
        .then(() => {
          assertFalse(getSessionLogButton().disabled);
        });
  });

  test('SaveSessionLogSuccessShowsToast', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(true);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.shadowRoot.querySelector('#toast'),
                loadTimeData.getString('sessionLogToastTextSuccess'));
          });
        });
  });

  test('SaveSessionLogFailure', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(false);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.shadowRoot.querySelector('#toast'),
                loadTimeData.getString('sessionLogToastTextFailure'));
          });
        });
  });

  test('SessionLogHiddenWhenNotLoggedIn', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (false)))
        .then(() => assertFalse(isVisible(getSessionLogButton())));
  });

  test('SessionLogShownWhenLoggedIn', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (true)))
        .then(() => assertTrue(isVisible(getSessionLogButton())));
  });

  // System page is only responsible for banner display when in stand-alone
  // view.
  if (!window.isNetworkEnabled) {
    test('RunningCpuTestsShowsBanner', () => {
      /** @type {?RoutineSectionElement} */
      let routineSection;
      /** @type {!Array<!RoutineType>} */
      const routines = [
        RoutineType.kCpuCache,
      ];
      routineController.setFakeStandardRoutineResult(
          RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
      return initializeSystemPage(
                 fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
                 fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
          .then(() => {
            routineSection = dx_utils.getRoutineSection(
                page.shadowRoot.querySelector('cpu-card'));
            routineSection.routines = routines;
            assertFalse(isVisible(getCautionBanner()));
            return flushTasks();
          })
          .then(() => {
            dx_utils.getRunTestsButtonFromSection(routineSection).click();
            return flushTasks();
          })
          .then(() => {
            assertTrue(isVisible(getCautionBanner()));
            return routineController.resolveRoutineForTesting();
          })
          .then(() => flushTasks())
          .then(() => assertFalse(isVisible(getCautionBanner())));
    });

    test('RunningMemoryTestsShowsBanner', () => {
      /** @type {?RoutineSectionElement} */
      let routineSection;
      /** @type {!Array<!RoutineType>} */
      const routines = [RoutineType.kMemory];
      routineController.setFakeStandardRoutineResult(
          RoutineType.kMemory, StandardRoutineResult.kTestPassed);
      return initializeSystemPage(
                 fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
                 fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
          .then(() => {
            routineSection = dx_utils.getRoutineSection(
                page.shadowRoot.querySelector('memory-card'));
            routineSection.routines = routines;
            assertFalse(isVisible(getCautionBanner()));
            return flushTasks();
          })
          .then(() => {
            dx_utils.getRunTestsButtonFromSection(routineSection).click();
            return flushTasks();
          })
          .then(() => {
            dx_utils.assertElementContainsText(
                page.shadowRoot.querySelector('#banner > #bannerMsg'),
                loadTimeData.getString('memoryBannerMessage'));
            assertTrue(isVisible(getCautionBanner()));
            return routineController.resolveRoutineForTesting();
          })
          .then(() => flushTasks())
          .then(() => assertFalse(isVisible(getCautionBanner())));
    });
  }

  test('RecordNavigationCalled', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          page.onNavigationPageChanged({isActive: false});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              0, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));

          DiagnosticsBrowserProxy.setPreviousView(NavigationView.CONNECTIVITY);
          page.onNavigationPageChanged({isActive: true});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              1, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));
          assertArrayEquals(
              [NavigationView.CONNECTIVITY, NavigationView.SYSTEM],
              /** @type {!Array<!NavigationView>} */
              (DiagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
        });
  });
}
