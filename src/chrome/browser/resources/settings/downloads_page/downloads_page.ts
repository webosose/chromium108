// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {WebUIListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {DownloadsBrowserProxy, DownloadsBrowserProxyImpl} from './downloads_browser_proxy.js';
import {getTemplate} from './downloads_page.html.js';

interface AccountInfo {
  linked: boolean;
  account: {name: string, login: string};
  folder: {name: string, link: string};
}

const SettingsDownloadsPageElementBase =
    WebUIListenerMixin(PrefsMixin(PolymerElement));

export class SettingsDownloadsPageElement extends
    SettingsDownloadsPageElementBase {
  static get is() {
    return 'settings-downloads-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      showConnection_: {
        type: Boolean,
        value: false,
      },

      connectionLearnMoreLink_: {
        type: String,
        value:
            'https://chromeenterprise.google/policies/?policy=SendDownloadToCloudEnterpriseConnector',
      },

      /**
       * The connection account info object. The definition is based on
       * chrome/browser/enterprise/connectors/file_system/signin_experience.cc:
       * GetFileSystemConnectorLinkedAccountInfoForSettingsPage()
       */
      connectionAccountInfo_: {
        type: Object,
        notify: true,
      },

      connectionSetupInProgress_: {
        type: Boolean,
        value: false,
      },

      autoOpenDownloads_: {
        type: Boolean,
        value: false,
      },

      // <if expr="chromeos_ash">
      /**
       * The download location string that is suitable to display in the UI.
       */
      downloadLocation_: String,
      // </if>
    };
  }

  // <if expr="chromeos_ash">
  static get observers() {
    return [
      'handleDownloadLocationChanged_(prefs.download.default_directory.value)',
    ];
  }
  // </if>


  private showConnection_: boolean;
  private connectionLearnMoreLink_: string;
  private connectionAccountInfo_: AccountInfo;
  private connectionSetupInProgress_: boolean;
  private autoOpenDownloads_: boolean;

  // <if expr="chromeos_ash">
  private downloadLocation_: string;
  // </if>

  private browserProxy_: DownloadsBrowserProxy =
      DownloadsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUIListener(
        'auto-open-downloads-changed', (autoOpen: boolean) => {
          this.autoOpenDownloads_ = autoOpen;
        });

    this.addWebUIListener(
        'downloads-connection-policy-changed',
        (downloadsConnectionEnabled: boolean) => {
          this.showConnection_ = downloadsConnectionEnabled;
        });

    this.addWebUIListener(
        'downloads-connection-link-changed', (accountInfo: AccountInfo) => {
          this.connectionAccountInfo_ = accountInfo;
          this.connectionSetupInProgress_ = false;
          // Focus on the link/unlink button so that the updated linked account
          // status gets announced by screen reader.
          afterNextRender(this, () => {
            const button = this.connectionAccountInfo_.linked ?
                this.shadowRoot!.querySelector<HTMLElement>(
                    '#unlinkAccountButton') :
                this.shadowRoot!.querySelector<HTMLElement>(
                    '#linkAccountButton');
            focusWithoutInk(button!);
          });
        });

    this.browserProxy_.initializeDownloads();
  }

  private onLinkDownloadsConnectionClick_() {
    this.connectionSetupInProgress_ = true;
    this.browserProxy_.setDownloadsConnectionAccountLink(true);
  }

  private onUnlinkDownloadsConnectionClick_() {
    this.connectionSetupInProgress_ = true;
    this.browserProxy_.setDownloadsConnectionAccountLink(false);
  }

  private selectDownloadLocation_() {
    listenOnce(this, 'transitionend', () => {
      this.browserProxy_.selectDownloadLocation();
    });
  }

  // <if expr="chromeos_ash">
  private handleDownloadLocationChanged_() {
    this.browserProxy_
        .getDownloadLocationText(
            this.getPref('download.default_directory').value)
        .then(text => {
          this.downloadLocation_ = text;
        });
  }
  // </if>

  private onClearAutoOpenFileTypesTap_() {
    this.browserProxy_.resetAutoOpenFileTypes();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-downloads-page': SettingsDownloadsPageElement;
  }
}

customElements.define(
    SettingsDownloadsPageElement.is, SettingsDownloadsPageElement);
