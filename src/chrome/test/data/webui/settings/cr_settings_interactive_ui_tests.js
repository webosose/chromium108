// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 elements. */
var CrSettingsInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }
};

var CrSettingsAnimatedPagesTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_animated_pages_test.js';
  }
};

TEST_F('CrSettingsAnimatedPagesTest', 'All', function() {
  mocha.run();
});

var CrSettingsPaymentsSectionTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/payments_section_interactive_test.js';
  }
};

TEST_F('CrSettingsPaymentsSectionTest', 'All', function() {
  mocha.run();
});

var CrSettingsSyncPageTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/people_page_sync_page_interactive_test.js';
  }
};

TEST_F('CrSettingsSyncPageTest', 'All', function() {
  mocha.run();
});

var CrSettingsSecureDnsTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/secure_dns_interactive_test.js';
  }
};

TEST_F('CrSettingsSecureDnsTest', 'All', function() {
  mocha.run();
});

var SettingsUIInteractiveTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_ui_tests.js';
  }
};

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_SettingsUIToolbarAndDrawer DISABLED_SettingsUIToolbarAndDrawer');
GEN('#else');
GEN('#define MAYBE_SettingsUIToolbarAndDrawer SettingsUIToolbarAndDrawer');
GEN('#endif');
TEST_F(
    'SettingsUIInteractiveTest', 'MAYBE_SettingsUIToolbarAndDrawer',
    function() {
      runMochaSuite('SettingsUIToolbarAndDrawer');
    });

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_SettingsUISearch DISABLED_SettingsUISearch');
GEN('#else');
GEN('#define MAYBE_SettingsUISearch SettingsUISearch');
GEN('#endif');
TEST_F('SettingsUIInteractiveTest', 'MAYBE_SettingsUISearch', function() {
  runMochaSuite('SettingsUISearch');
});

var CrSettingsMenuInteractiveTest = class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_menu_interactive_ui_test.js';
  }
};

TEST_F('CrSettingsMenuInteractiveTest', 'All', function() {
  mocha.run();
});

var CrSettingsTextareaInteractiveTest =
    class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return `chrome://settings/test_loader.html?module=settings/settings_textarea_tests.js`;
  }
};

TEST_F('CrSettingsTextareaInteractiveTest', 'All', function() {
  mocha.run();
});

var CrSettingsReviewNotificationPermissionsInteractiveUITest =
    class extends CrSettingsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/review_notification_permissions_interactive_ui_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSafetyCheckNotificationPermissions',
      ],
    };
  }
};

TEST_F(
    'CrSettingsReviewNotificationPermissionsInteractiveUITest', 'All',
    function() {
      mocha.run();
    });