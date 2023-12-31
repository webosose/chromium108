// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Family Link Notice screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const FamilyLinkScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   familyLinkDialog:  OobeAdaptiveDialogElement,
 * }}
 */
FamilyLinkScreenElementBase.$;

class FamilyLinkNotice extends FamilyLinkScreenElementBase {
  static get is() {
    return 'family-link-notice-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * If the gaia account is newly created
       */
      isNewGaiaAccount_: {
        type: Boolean,
      },

      /**
       * The email address to be displayed
       */
      email_: {
        type: String,
      },

      /**
       * The enterprise domain to be displayed
       */
      domain_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.isNewGaiaAccount_ = false;
    this.email_ = '';
    this.domain_ = '';
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return [
      'setDisplayEmail',
      'setDomain',
      'setIsNewGaiaAccount',
    ];
  }

  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('FamilyLinkNoticeScreen');
  }

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.familyLinkDialog;
  }

  /**
   * Sets email address.
   * @param {string} email
   */
  setDisplayEmail(email) {
    this.email_ = email;
  }

  /**
   * Sets enterprise domain.
   * @param {string} domain
   */
  setDomain(domain) {
    this.domain_ = domain;
  }

  /**
   * Sets if the gaia account is newly created.
   * @param {boolean} isNewGaiaAccount
   */
  setIsNewGaiaAccount(isNewGaiaAccount) {
    this.isNewGaiaAccount_ = isNewGaiaAccount;
  }

  /**
   * Returns the title of the dialog based on if account is managed. Account is
   * managed when email or domain field is not empty and we show parental
   * controls is not eligible.
   *
   * @private
   */
  getDialogTitle_(locale, email, domain) {
    if (email || domain) {
      return this.i18n('familyLinkDialogNotEligibleTitle');
    } else {
      return this.i18n('familyLinkDialogTitle');
    }
  }

  /**
   * Formats and returns the subtitle of the dialog based on if account is
   * managed or if account is newly created. Account is managed when email or
   * domain field is not empty and we show parental controls is not eligible.
   *
   * @private
   */
  getDialogSubtitle_(locale, isNewGaiaAccount, email, domain) {
    if (email || domain) {
      return this.i18n('familyLinkDialogNotEligibleSubtitle', email, domain);
    } else {
      if (isNewGaiaAccount) {
        return this.i18n('familyLinkDialogNewGaiaAccountSubtitle');
      } else {
        return this.i18n('familyLinkDialogExistingGaiaAccountSubtitle');
      }
    }
  }

  /**
   * On-tap event handler for Continue button.
   *
   * @private
   */
  onContinueButtonPressed_() {
    this.userActed('continue');
  }
}

customElements.define(FamilyLinkNotice.is, FamilyLinkNotice);
