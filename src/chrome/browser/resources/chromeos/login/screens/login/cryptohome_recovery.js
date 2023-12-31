// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/common_styles/common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.m.js';
import '../../components/dialogs/oobe_loading_dialog.m.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.m.js';

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const CryptohomeRecoveryUIState = {
  LOADING: 'loading',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const CryptohomeRecoveryBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class CryptohomeRecovery extends CryptohomeRecoveryBase {
  static get is() {
    return 'cryptohome-recovery-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  defaultUIStep() {
    return CryptohomeRecoveryUIState.LOADING;
  }

  get UI_STEPS() {
    return CryptohomeRecoveryUIState;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('CryptohomeRecoveryScreen');
  }

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {}

  reset() {
    this.setUIStep(CryptohomeRecoveryUIState.LOADING);
  }
}

customElements.define(CryptohomeRecovery.is, CryptohomeRecovery);
