// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/multi_page_checkbox.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';

export function multiPageCheckboxTest() {
  /** @type {?MultiPageCheckboxElement} */
  let multiPageCheckbox = null;

  setup(() => {
    multiPageCheckbox = /** @type {!MultiPageCheckboxElement} */ (
        document.createElement('multi-page-checkbox'));
    assertTrue(!!multiPageCheckbox);
    document.body.appendChild(multiPageCheckbox);
  });

  teardown(() => {
    if (multiPageCheckbox) {
      multiPageCheckbox.remove();
    }
    multiPageCheckbox = null;
  });

  // Verify that clicking the checkbox directly and clicking the text label can
  // both toggle the boolean.
  test('checkboxClicked', () => {
    assertFalse(multiPageCheckbox.multiPageScanChecked);
    multiPageCheckbox.$$('cr-checkbox').click();
    assertTrue(multiPageCheckbox.multiPageScanChecked);
    multiPageCheckbox.$$('#checkboxText').click();
    assertFalse(multiPageCheckbox.multiPageScanChecked);
  });
}
