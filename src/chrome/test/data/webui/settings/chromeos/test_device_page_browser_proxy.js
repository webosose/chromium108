// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NoteAppLockScreenSupport} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * @implements {DevicePageBrowserProxy}
 */
export class TestDevicePageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showPlayStore',
    ]);

    this.keyboardShortcutViewerShown_ = 0;
    this.updatePowerStatusCalled_ = 0;
    this.requestPowerManagementSettingsCalled_ = 0;
    this.requestNoteTakingApps_ = 0;
    this.onNoteTakingAppsUpdated_ = null;

    this.androidAppsReceived_ = false;
    this.noteTakingApps_ = [];
    this.setPreferredAppCount_ = 0;
    this.setAppOnLockScreenCount_ = 0;

    this.lastHighlightedDisplayId_ = '-1';
  }

  initializePointers() {
    // Enable mouse and touchpad.
    webUIListenerCallback('has-mouse-changed', true);
    webUIListenerCallback('has-pointing-stick-changed', true);
    webUIListenerCallback('has-touchpad-changed', true);
    webUIListenerCallback('has-haptic-touchpad-changed', true);
  }

  initializeStylus() {
    // Enable stylus.
    webUIListenerCallback('has-stylus-changed', true);
  }

  initializeKeyboard() {}

  showKeyboardShortcutViewer() {
    this.keyboardShortcutViewerShown_++;
  }

  updateAndroidEnabled() {}

  updatePowerStatus() {
    this.updatePowerStatusCalled_++;
  }

  setPowerSource(powerSourceId) {
    this.powerSourceId_ = powerSourceId;
  }

  requestPowerManagementSettings() {
    this.requestPowerManagementSettingsCalled_++;
  }

  setIdleBehavior(behavior, whenOnAc) {
    if (whenOnAc) {
      this.acIdleBehavior_ = behavior;
    } else {
      this.batteryIdleBehavior_ = behavior;
    }
  }

  setLidClosedBehavior(behavior) {
    this.lidClosedBehavior_ = behavior;
  }

  setNoteTakingAppsUpdatedCallback(callback) {
    this.onNoteTakingAppsUpdated_ = callback;
  }

  requestNoteTakingApps() {
    this.requestNoteTakingApps_++;
  }

  setPreferredNoteTakingApp(appId) {
    ++this.setPreferredAppCount_;

    let changed = false;
    this.noteTakingApps_.forEach((app) => {
      changed = changed || app.preferred !== (app.value === appId);
      app.preferred = app.value === appId;
    });

    if (changed) {
      this.scheduleLockScreenAppsUpdated_();
    }
  }

  setPreferredNoteTakingAppEnabledOnLockScreen(enabled) {
    ++this.setAppOnLockScreenCount_;

    this.noteTakingApps_.forEach((app) => {
      if (enabled) {
        if (app.preferred) {
          assertEquals(
              NoteAppLockScreenSupport.SUPPORTED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.SUPPORTED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.ENABLED;
        }
      } else {
        if (app.preferred) {
          assertEquals(NoteAppLockScreenSupport.ENABLED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.ENABLED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.SUPPORTED;
        }
      }
    });

    this.scheduleLockScreenAppsUpdated_();
  }

  highlightDisplay(id) {
    this.lastHighlightedDisplayId_ = id;
  }

  updateStorageInfo() {}

  // Test interface:
  /**
   * Sets whether the app list contains Android apps.
   * @param {boolean} Whether the list of Android note-taking apps was
   *     received.
   */
  setAndroidAppsReceived(received) {
    this.androidAppsReceived_ = received;

    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * @return {string} App id of the app currently selected as preferred.
   */
  getPreferredNoteTakingAppId() {
    const app = this.noteTakingApps_.find(function(existing) {
      return existing.preferred;
    });

    return app ? app.value : '';
  }

  /**
   * @return {NoteAppLockScreenSupport | undefined} The lock screen
   *     support state of the app currently selected as preferred.
   */
  getPreferredAppLockScreenState() {
    const app = this.noteTakingApps_.find(function(existing) {
      return existing.preferred;
    });

    return app ? app.lockScreenSupport : undefined;
  }

  /**
   * Sets the current list of known note taking apps.
   * @param {Array<!NoteAppInfo>} The list of apps to set.
   */
  setNoteTakingApps(apps) {
    this.noteTakingApps_ = apps;
    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * Adds an app to the list of known note-taking apps.
   * @param {!NoteAppInfo}
   */
  addNoteTakingApp(app) {
    assert(!this.noteTakingApps_.find((existing) => {
      return existing.value === app.value;
    }));

    this.noteTakingApps_.push(app);
    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * Invokes the registered note taking apps update callback.
   * @private
   */
  scheduleLockScreenAppsUpdated_() {
    this.onNoteTakingAppsUpdated_(
        this.noteTakingApps_.map((app) => {
          return Object.assign({}, app);
        }),
        !this.androidAppsReceived_);
  }

  showPlayStore(url) {
    this.methodCalled(this.showPlayStore.name, url);
  }
}
