// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from '../../chai_assert.js';

suite('TimezoneSelectorTests', function() {
  /** @type {TimezoneSelector} */
  let timezoneSelector = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    timezoneSelector.remove();
  });

  test('Per-user timezone disabled', async () => {
    timezoneSelector = document.createElement('timezone-selector');
    timezoneSelector.prefs = {
      'cros': {
        'flags': {
          'per_user_timezone_enabled': {
            value: false,
          },
        },
      },
    };
    document.body.appendChild(timezoneSelector);

    flush();

    assertEquals(
        null,
        timezoneSelector.shadowRoot.querySelector('#userTimeZoneSelector'));
    assertEquals(
        null,
        timezoneSelector.shadowRoot.querySelector('#systemTimezoneSelector'));
  });

  test('Per-user timezone enabled', async () => {
    timezoneSelector = document.createElement('timezone-selector');
    timezoneSelector.prefs = {
      'cros': {
        'flags': {
          'per_user_timezone_enabled': {
            value: true,
          },
        },
      },
    };
    document.body.appendChild(timezoneSelector);

    flush();

    const userTimezoneSelector = assert(
        timezoneSelector.shadowRoot.querySelector('#userTimeZoneSelector'));
    const systemTimezoneSelector = assert(
        timezoneSelector.shadowRoot.querySelector('#systemTimezoneSelector'));
  });
});
