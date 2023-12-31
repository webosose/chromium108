// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_card_frame.js';
import './icons.html.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './input_card.html.js';
import {ConnectionType, KeyboardInfo, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';

declare global {
  interface HTMLElementEventMap {
    'test-button-click': CustomEvent<{evdevId: number}>;
  }
}

/**
 * @fileoverview
 * 'input-card' is responsible for displaying a list of input devices with links
 * to their testers.
 */

/**
 * Enum of device types supported by input-card elements.
 */
export enum InputCardType {
  KEYBOARD = 'keyboard',
  TOUCHPAD = 'touchpad',
  TOUCHSCREEN = 'touchscreen',
}

const InputCardElementBase = I18nMixin(PolymerElement);

export class InputCardElement extends InputCardElementBase {
  static get is() {
    return 'input-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The type of input device to be displayed. Valid values are 'keyboard',
       * 'touchpad', and 'touchscreen'.
       */
      deviceType: String,

      devices: {
        type: Array,
        value: () => [],
      },

      deviceIcon_: {
        type: String,
        computed: 'computeDeviceIcon_(deviceType)',
      },
    };
  }

  deviceType: InputCardType;
  devices: KeyboardInfo[]|TouchDeviceInfo[];
  private deviceIcon_: string;

  private computeDeviceIcon_(deviceType: InputCardType): string {
    return {
      [InputCardType.KEYBOARD]: 'diagnostics:keyboard',
      [InputCardType.TOUCHPAD]: 'diagnostics:touchpad',
      [InputCardType.TOUCHSCREEN]: 'diagnostics:touchscreen',
    }[deviceType];
  }

  /**
   * Fetches the description string for a device based on its connection type
   * (e.g. "Bluetooth keyboard", "Internal touchpad").
   */
  private getDeviceDescription_(device: KeyboardInfo|TouchDeviceInfo): string {
    if (device.connectionType === ConnectionType.kUnknown) {
      return '';
    }
    const connectionTypeString = {
      [ConnectionType.kInternal]: 'Internal',
      [ConnectionType.kUsb]: 'Usb',
      [ConnectionType.kBluetooth]: 'Bluetooth',
    }[device.connectionType];
    const deviceTypeString = {
      [InputCardType.KEYBOARD]: 'Keyboard',
      [InputCardType.TOUCHPAD]: 'Touchpad',
      [InputCardType.TOUCHSCREEN]: 'Touchscreen',
    }[this.deviceType];
    return loadTimeData.getString(
        'inputDescription' + connectionTypeString + deviceTypeString);
  }

  private handleTestButtonClick_(e: PointerEvent): void {
    const inputDeviceButton = e.target as CrButtonElement;
    assert(inputDeviceButton);
    const closestDevice: HTMLDivElement|null =
        inputDeviceButton.closest('.device');
    assert(closestDevice);
    const dataEvdevId = closestDevice.getAttribute('data-evdev-id');
    assert(typeof dataEvdevId === 'string');
    const evdevId = parseInt(dataEvdevId, 10);
    this.dispatchEvent(new CustomEvent(
        'test-button-click', {composed: true, detail: {evdevId: evdevId}}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-card': InputCardElement;
  }
}

customElements.define(InputCardElement.is, InputCardElement);
