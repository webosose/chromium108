// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// SetTimeDialogBrowserTest tests the "Set Time" web UI dialog.
var SetTimeDialogBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://set-time/test_loader.html?module=set_time_dialog_test.js&host=test';
  }
};

TEST_F('SetTimeDialogBrowserTest', 'All', function() {
  mocha.run();
});
