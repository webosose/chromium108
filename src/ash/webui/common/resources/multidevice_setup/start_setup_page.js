// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './mojo_api.js';
import './multidevice_setup_shared.css.js';
import './ui_page.js';
import '//resources/js/cr.m.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {WebUIListenerBehavior} from '//resources/ash/common/web_ui_listener_behavior.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ConnectivityStatus} from 'chrome://resources/mojo/ash/services/device_sync/public/mojom/device_sync.mojom-webui.js';
import {HostDevice} from 'chrome://resources/mojo/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

import {MultiDeviceSetupDelegate} from './multidevice_setup_delegate.js';
import {getTemplate} from './start_setup_page.html.js';
import {UiPageContainerBehavior} from './ui_page_container_behavior.js';

/**
 * The multidevice setup animation for light mode.
 * @type {string}
 */
const MULTIDEVICE_ANIMATION_DARK_URL = 'multidevice_setup_dark.json';

/**
 * The multidevice setup animation for dark mode.
 * @type {string}
 */
const MULTIDEVICE_ANIMATION_LIGHT_URL = 'multidevice_setup_light.json';

Polymer({
  _template: getTemplate(),
  is: 'start-setup-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'accept',
    },

    /** Overridden from UiPageContainerBehavior. */
    cancelButtonTextId: {
      type: String,
      computed: 'getCancelButtonTextId_(delegate)',
    },

    /**
     * Array of objects representing all potential MultiDevice hosts.
     *
     * @type {!Array<!HostDevice>}
     */
    devices: {
      type: Array,
      value: () => [],
      observer: 'devicesChanged_',
    },

    /**
     * Unique identifier for the currently selected host device. This uses the
     * device's Instance ID if it is available; otherwise, the device's legacy
     * device ID is used.
     * TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
     * use Instance ID since all devices are guaranteed to have one.
     *
     * Undefined if the no list of potential hosts has been received from mojo
     * service.
     *
     * @type {string|undefined}
     */
    selectedInstanceIdOrLegacyDeviceId: {
      type: String,
      notify: true,
    },

    /**
     * Delegate object which performs differently in OOBE vs. non-OOBE mode.
     * @type {!MultiDeviceSetupDelegate}
     */
    delegate: Object,

    /** @private */
    wifiSyncEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('wifiSyncEnabled') &&
            loadTimeData.getBoolean('wifiSyncEnabled');
      },
    },

    /** @private */
    phoneHubCameraRollEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('phoneHubCameraRollEnabled') &&
            loadTimeData.getBoolean('phoneHubCameraRollEnabled');
      },
    },

    /**
     * Whether the multidevice setup page is being rendered in dark mode.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  behaviors: [
    UiPageContainerBehavior,
    WebUIListenerBehavior,
  ],

  /** @override */
  attached() {
    this.addWebUIListener(
        'multidevice_setup.initializeSetupFlow',
        () => this.initializeSetupFlow_());
  },

  /**
   * This will play or stop the screen's lottie animation.
   * @param {boolean} enabled Whether the animation should play or not.
   */
  setPlayAnimation(enabled) {
    /** @type {!CrLottieElement} */ (this.$.multideviceSetupAnimation)
        .setPlay(enabled);
  },

  /** @private */
  initializeSetupFlow_() {
    // The "Learn More" links are inside a grdp string, so we cannot actually
    // add an onclick handler directly to the html. Instead, grab the two and
    // manaully add onclick handlers.
    const helpArticleLinks = [
      this.$$('#multidevice-summary-message a'),
    ];
    for (let i = 0; i < helpArticleLinks.length; i++) {
      helpArticleLinks[i].onclick = this.fire.bind(
          this, 'open-learn-more-webview-requested', helpArticleLinks[i].href);
    }
  },

  /**
   * @param {!MultiDeviceSetupDelegate} delegate
   * @return {string} The cancel button text ID, dependent on OOBE vs. non-OOBE.
   * @private
   */
  getCancelButtonTextId_(delegate) {
    return this.delegate.getStartSetupCancelButtonTextId();
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {string} Label for devices selection content.
   * @private
   */
  getDeviceSelectionHeader_(devices) {
    switch (devices.length) {
      case 0:
        return '';
      case 1:
        return this.i18n('startSetupPageSingleDeviceHeader');
      default:
        return this.i18n('startSetupPageMultipleDeviceHeader');
    }
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {boolean} True if there are more than one potential host devices.
   * @private
   */
  doesDeviceListHaveMultipleElements_(devices) {
    return devices.length > 1;
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {boolean} True if there is exactly one potential host device.
   * @private
   */
  doesDeviceListHaveOneElement_(devices) {
    return devices.length === 1;
  },

  /**
   * @param {!Array<!HostDevice>} devices
   * @return {string} Name of the first device in device list if there are any.
   *     Returns an empty string otherwise.
   * @private
   */
  getFirstDeviceNameInList_(devices) {
    return devices[0] ? this.devices[0].remoteDevice.deviceName : '';
  },

  /**
   * @param {!ConnectivityStatus} connectivityStatus
   * @return {string} The classes to bind to the device name option.
   * @private
   */
  getDeviceOptionClass_(connectivityStatus) {
    return connectivityStatus === ConnectivityStatus.kOffline ?
        'offline-device-name' :
        '';
  },

  /**
   * @param {!HostDevice} device
   * @return {string} Name of the device, with connectivity status information.
   * @private
   */
  getDeviceNameWithConnectivityStatus_(device) {
    return device.connectivityStatus === ConnectivityStatus.kOffline ?
        this.i18n(
            'startSetupPageOfflineDeviceOption',
            device.remoteDevice.deviceName) :
        device.remoteDevice.deviceName;
  },

  /**
   * @param {!HostDevice} device
   * @return {string} Returns a unique identifier for the input device, using
   *     the device's Instance ID if it is available; otherwise, the device's
   *     legacy device ID is used.
   *     TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
   *     use Instance ID since all devices are guaranteed to have one.
   * @private
   */
  getInstanceIdOrLegacyDeviceId_(device) {
    if (device.remoteDevice.instanceId) {
      return device.remoteDevice.instanceId;
    }

    return device.remoteDevice.deviceId;
  },

  /** @private */
  devicesChanged_() {
    if (this.devices.length > 0) {
      this.selectedInstanceIdOrLegacyDeviceId =
          this.getInstanceIdOrLegacyDeviceId_(this.devices[0]);
    }
  },

  /** @private */
  onDeviceDropdownSelectionChanged_() {
    this.selectedInstanceIdOrLegacyDeviceId = this.$.deviceDropdown.value;
  },

  /**
   * Wrapper for i18nAdvanced for binding to location updates in OOBE.
   * @param {string} locale The language code (e.g. en, es) for the current
   *     display language for CrOS. As with I18nBehavior.i18nDynamic(), the
   *     parameter is not used directly but is provided to allow HTML binding
   *     without passing an unexpected argument to I18nBehavior.i18nAdvanced().
   * @param {string} textId The loadTimeData ID of the string to be translated.
   * @private
   */
  i18nAdvancedDynamic_(locale, textId) {
    return this.i18nAdvanced(textId);
  },

  /**
   * Returns the URL for the asset that defines the multidevice setup page's
   * animation
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? MULTIDEVICE_ANIMATION_DARK_URL :
                                    MULTIDEVICE_ANIMATION_LIGHT_URL;
  },
});
