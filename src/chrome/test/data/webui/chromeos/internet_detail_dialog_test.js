// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://internet-detail-dialog/internet_detail_dialog_container.js';

import {InternetDetailDialogBrowserProxyImpl} from 'chrome://internet-detail-dialog/internet_detail_dialog_container.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {InternetDetailDialogBrowserProxy} */
export class TestInternetDetailDialogBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDialogArguments',
      'closeDialog',
      'showPortalSignin',
    ]);
  }

  /** @override */
  getDialogArguments() {
    return JSON.stringify({guid: 'guid'});
  }

  /** @override */
  closeDialog() {}

  /** @override */
  showPortalSignin() {}
}

suite('internet-detail-dialog', () => {
  const guid = 'guid';
  const test_iccid = '11111111111111111';
  let internetDetailDialog = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function getManagedProperties(type, opt_source) {
    const result = OncMojo.getDefaultManagedProperties(type, guid, name);
    if (opt_source) {
      result.source = opt_source;
    }
    return result;
  }

  setup(async () => {
    PolymerTest.clearBody();
    InternetDetailDialogBrowserProxyImpl.instance_ =
        new TestInternetDetailDialogBrowserProxy();
    mojoApi_.resetForTest();
  });

  async function init(captive_portal_2022) {
    internetDetailDialog = document.createElement('internet-detail-dialog');
    internetDetailDialog.isCaptivePortalUI2022Enabled_ = captive_portal_2022;
    document.body.appendChild(internetDetailDialog);
    await flushAsync();
  }

  async function setupCellularNetwork(isPrimary, isInhibited) {
    await mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);

    const cellularNetwork =
        getManagedProperties(NetworkType.kCellular, OncSource.kDevice);
    cellularNetwork.typeProperties.cellular.iccid = test_iccid;
    // Required for connectDisconnectButton to be rendered.
    cellularNetwork.connectionState = isPrimary ?
        ConnectionStateType.kConnected :
        ConnectionStateType.kNotConnected;
    // Required for networkChooseMobile to be rendered.
    cellularNetwork.typeProperties.cellular.supportNetworkScan = true;

    mojoApi_.setManagedPropertiesForTest(cellularNetwork);
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason:
          (isInhibited ? InhibitReason.kInstallingProfile :
                         InhibitReason.kNotInhibited),
      simInfos: [{
        iccid: test_iccid,
        isPrimary: isPrimary,
      }],
    });
  }

  function getElement(selector) {
    const element = internetDetailDialog.$$(selector);
    assertTrue(!!element);
    return element;
  }

  suite('captive portal ui updates', () => {
    function getButton(buttonId) {
      const button =
          internetDetailDialog.shadowRoot.querySelector(`#${buttonId}`);
      assertTrue(!!button);
      return button;
    }

    test('WiFi in a portal portalState', function() {
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortal;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);
      init(/*captive_portal_2022=*/ true);
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailDialog.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });

    test('WiFi in a no internet portalState', function() {
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kNoInternet;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);
      init(/*captive_portal_2022=*/ true);
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailDialog.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n(
                'networkListItemConnectedNoConnectivity'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertTrue(signinButton.hasAttribute('hidden'));
        assertTrue(signinButton.disabled);
      });
    });

    test('WiFi in a proxy-auth portalState', function() {
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kProxyAuthRequired;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);
      init(/*captive_portal_2022=*/ true);
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailDialog.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });

    test('WiFi in a portal portalState and feature flag disabled', function() {
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortal;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);
      init(/*captive_portal_2022=*/ false);
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailDialog.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('connected'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n('OncConnected'));
        const signinButton =
            internetDetailDialog.shadowRoot.querySelector(`#signinButton`);
        // Button does not exist because feature flag is disabled.
        assertTrue(!signinButton);
      });
    });
  });

  test('Network not on active sim, hide configurations', async () => {
    await setupCellularNetwork(/*isPrimary=*/ false, /*isInhibited=*/ false);

    await init();
    assertFalse(internetDetailDialog.showConfigurableSections_);

    const managedProperties = internetDetailDialog.managedProperties_;
    assertTrue(internetDetailDialog.showCellularSim_(managedProperties));
    assertFalse(!!internetDetailDialog.$$('network-siminfo'));

    // The 'Forget' and 'ConnectDisconnect' buttons should still be showing.
    assertTrue(!!internetDetailDialog.$$('cr-button'));
  });

  test('Network on active sim, show configurations', async () => {
    await setupCellularNetwork(/*isPrimary=*/ true, /*isInhibited=*/ false);

    await init();
    assertTrue(internetDetailDialog.showConfigurableSections_);

    const managedProperties = internetDetailDialog.managedProperties_;
    assertTrue(internetDetailDialog.showCellularSim_(managedProperties));
    assertTrue(!!internetDetailDialog.$$('network-siminfo'));
  });

  test('Dialog disabled when inhibited', async () => {
    // Start uninhibited.
    await setupCellularNetwork(/*isPrimary=*/ true, /*isInhibited=*/ false);
    await init();

    const connectDisconnectButton = getElement('#connectDisconnect');
    const networkSimInfo = getElement('network-siminfo');
    const networkChooseMobile = getElement('network-choose-mobile');
    const networkApnlist = getElement('network-apnlist');
    const networkProxy = getElement('network-proxy');
    const networkIpConfig = getElement('network-ip-config');
    const networkNameservers = getElement('network-nameservers');
    const infoFields = getElement('network-property-list-mojo');

    assertFalse(connectDisconnectButton.disabled);
    assertFalse(networkSimInfo.disabled);
    assertFalse(networkChooseMobile.disabled);
    assertFalse(networkApnlist.disabled);
    assertTrue(networkProxy.editable);
    assertFalse(networkIpConfig.disabled);
    assertFalse(networkNameservers.disabled);
    assertFalse(infoFields.disabled);

    // Mock device being inhibited.
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kInstallingProfile,
      simInfos: [{
        iccid: test_iccid,
        isPrimary: true,
      }],
    });
    await flushAsync();

    assertTrue(connectDisconnectButton.disabled);
    assertTrue(networkSimInfo.disabled);
    assertTrue(networkChooseMobile.disabled);
    assertTrue(networkApnlist.disabled);
    assertFalse(networkProxy.editable);
    assertTrue(networkIpConfig.disabled);
    assertTrue(networkNameservers.disabled);
    assertTrue(infoFields.disabled);

    // Uninhibit.
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      simInfos: [{
        iccid: test_iccid,
        isPrimary: true,
      }],
    });
    await flushAsync();

    assertFalse(connectDisconnectButton.disabled);
    assertFalse(networkSimInfo.disabled);
    assertFalse(networkChooseMobile.disabled);
    assertFalse(networkApnlist.disabled);
    assertTrue(networkProxy.editable);
    assertFalse(networkIpConfig.disabled);
    assertFalse(networkNameservers.disabled);
    assertFalse(infoFields.disabled);
  });
});
