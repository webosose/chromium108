// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementStore, createEmptyState, reduceAction} from 'chrome://os-settings/chromeos/os_settings.js';
import {TestStore} from 'chrome://test/chromeos/test_store.js';

export class TestAppManagementStore extends TestStore {
  constructor(data) {
    super(data, AppManagementStore, createEmptyState(), reduceAction);
  }

  /**
   * Replaces the global store instance with this TestStore. Overrides the
   * default implementation by using the setInstance() static method
   * @override
   */
  replaceSingleton() {
    AppManagementStore.setInstanceForTesting(this);
  }
}
