// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkSummary', function() {
  /** @type {!NetworkSummaryElement|undefined} */
  let netSummary;

  setup(function() {
    netSummary = document.createElement('network-summary');
    document.body.appendChild(netSummary);
    flush();
  });

  test('Default network summary item', function() {
    const summaryItems =
        netSummary.shadowRoot.querySelectorAll('network-summary-item');
    assertEquals(1, summaryItems.length);
    assertEquals('WiFi', summaryItems[0].id);
  });
});
