// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-modal-dialog',

  behaviors: [OobeI18nBehavior],

  properties: {
    /* The ID of the localized string to be used as title text when no "title"
     * slot elements are specified.
     */
    titleKey: {
      type: String,
    },
    /* The ID of the localized string to be used as the content when no
     * "content" slot elements are specified.
     */
    contentKey: {
      type: String,
    },

    /**
     * True if close button should be hidden.
     * @type {boolean}
     */
    shouldHideCloseButton: {
      type: Boolean,
      value: false,
    },

    /**
     * True if title row should be hidden.
     * @type {boolean}
     */
    shouldHideTitleRow: {
      type: Boolean,
      value: false,
    },
  },

  get open() {
    return this.$.modalDialog.open;
  },

  ready() {},

  /* Shows the modal dialog and changes the focus to the close button. */
  showDialog() {
    chrome.send('enableShelfButtons', [false]);
    this.$.modalDialog.showModal();
    this.$.closeButton.focus();
  },

  hideDialog() {
    this.$.modalDialog.close();
  },

  onClose_() {
    chrome.send('enableShelfButtons', [true]);
  },

});
