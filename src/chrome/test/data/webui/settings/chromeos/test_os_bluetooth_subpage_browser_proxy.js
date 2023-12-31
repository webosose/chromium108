// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {FastPairSavedDevicesOptInStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @implements {OsBluetoothDevicesSubpageBrowserProxy}
 */
export class TestOsBluetoothDevicesSubpageBrowserProxy extends
    TestBrowserProxy {
  constructor() {
    super([
      'requestFastPairSavedDevices',
      'deleteFastPairSavedDevice',
    ]);
    this.savedDevices = [];
    this.optInStatus = FastPairSavedDevicesOptInStatus.STATUS_OPTED_IN;
  }
  /** @override */
  requestFastPairSavedDevices() {
    this.methodCalled('requestFastPairSavedDevices');
    webUIListenerCallback('fast-pair-saved-devices-list', this.savedDevices);
    webUIListenerCallback(
        'fast-pair-saved-devices-opt-in-status', this.optInStatus);
  }

  /**
   * @override
   * @param {string} accountKey
   */
  deleteFastPairSavedDevice(accountKey) {
    // Remove the device from the proxy's device list if it exists,
    this.savedDevices =
        this.savedDevices.filter(device => device.accountKey !== accountKey);
  }
}
