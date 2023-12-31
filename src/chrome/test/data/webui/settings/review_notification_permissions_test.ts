// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {SettingsReviewNotificationPermissionsElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrActionMenuElement} from 'chrome://settings/settings.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('CrSettingsReviewNotificationPermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  let testElement: SettingsReviewNotificationPermissionsElement;

  const origin1 = 'https://www.example1.com:443';
  const detail1 = 'About 4 notifications a day';
  const origin2 = 'https://www.example2.com:443';
  const detail2 = 'About 1 notification a day';

  const mockData = [
    {
      origin: origin1,
      notificationInfoString: detail1,
    },
    {
      origin: origin2,
      notificationInfoString: detail2,
    },
  ];

  function assertNotification(
      toastShouldBeOpen: boolean, toastText?: string): void {
    const undoToast = testElement.$.undoToast;
    if (!toastShouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    assertEquals(testElement.$.undoNotification.textContent!.trim(), toastText);
  }

  /**
   * Clicks the Undo button and verifies that the correct origins are given to
   * the browser proxy call.
   */
  async function assertUndo(expectedProxyCall: string, index: number) {
    const entries = getEntries();
    const expectedOrigin =
        entries[index]!.querySelector(
                           '.site-representation')!.textContent!.trim();
    browserProxy.resetResolver(expectedProxyCall);
    testElement.$.undoToast.querySelector('cr-button')!.click();
    const origins = await browserProxy.whenCalled(expectedProxyCall);
    assertEquals(origins[0], expectedOrigin);
    assertNotification(false);
  }

  /* Asserts for each row whether or not it is animating. */
  function assertAnimation(expectedAnimation: boolean[]) {
    const rows = getEntries();

    assertEquals(
        rows.length, expectedAnimation.length,
        'Provided ' + expectedAnimation.length +
            ' expectations but there are ' + rows.length + ' rows');
    for (let i = 0; i < rows.length; ++i) {
      assertEquals(
          expectedAnimation[i]!, rows[i]!.classList.contains('removed'),
          'Expectation not met for row #' + i);
    }
  }

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    browserProxy.setNotificationPermissionReview(mockData);
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    testElement = document.createElement('review-notification-permissions');
    document.body.appendChild(testElement);

    // The model update delay exists to allow animations to finish before
    // the list is updated. As most of the test cases are not dependent on
    // animation visuals and this delay would slow down test execution,
    // set it to zero.
    testElement.setModelUpdateDelayMsForTesting(0);

    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  /**
   * Opens the action menu for a particular element in the list.
   * @param index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index: number) {
    const menu1 = testElement.shadowRoot!.querySelector('cr-action-menu')!;
    assertFalse(isVisible(menu1.getDialog()));

    const item = getEntries()[index]!;
    (item.querySelector('#actionMenuButton')! as HTMLElement).click();
    flush();

    const menu2 = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    assertTrue(isVisible(menu2.getDialog()));
  }

  function getEntries() {
    return testElement.shadowRoot!.querySelectorAll(
        '.notification-permissions-list .site-entry');
  }

  test('Notification Permission strings', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = getEntries();
    assertEquals(2, entries.length);

    // Check that the text describing the changed permissions is correct.
    assertEquals(
        origin1,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail1,
        entries[0]!.querySelector('.second-line')!.textContent!.trim());
    assertEquals(
        origin2,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail2,
        entries[1]!.querySelector('.second-line')!.textContent!.trim());
  });

  /**
   * Tests whether clicking on the block button results in the appropriate
   * browser proxy call and shows the notification toast element.
   */
  test('Dont Allow Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks don't allow.
    const element = entries[0]!.querySelector('#block')! as HTMLElement;
    element.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            expectedOrigin));
  });

  /**
   * Tests whether clicking on the ignore action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Ignore Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks ignore.
    openActionMenu(0);
    const reset =
        testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!;
    reset.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('ignoreNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    const dialog = menu.getDialog() as HTMLDialogElement;
    assertFalse(isVisible(dialog));
  });

  /**
   * Tests whether clicking on the reset action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Reset Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks reset.
    openActionMenu(0);
    const reset = testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!;
    reset.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('resetNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    const dialog = menu.getDialog() as HTMLDialogElement;
    assertFalse(isVisible(dialog));
  });

  /**
   * Tests whether clicking the Undo button after blocking a site correctly
   * resets the site to allow notifications and makes the toast element
   * disappear.
   */
  test('Undo Block Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    // Click block button
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.site-entry #block')!.click();
    assertAnimation([true, false]);
    await browserProxy.whenCalled('blockNotificationPermissionForOrigins');

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', mockData);
    assertAnimation([false, false]);
  });

  /**
   * Tests whether clicking the Undo button after ignoring notification a site
   * for permission review correctly removes the site from the blocklist
   * and makes the toast element disappear.
   */
  test('Undo Ignore Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    openActionMenu(0);
    // Click ignore button.
    testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!.click();
    assertAnimation([true, false]);

    await browserProxy.whenCalled('ignoreNotificationPermissionForOrigins');

    await assertUndo('undoIgnoreNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', mockData);
    assertAnimation([false, false]);
  });

  /**
   * Tests whether clicking the Undo button after resetting notification
   * permissions for a site correctly resets the site to allow notifications
   * and makes the toast element disappear.
   */
  test('Undo Reset Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    openActionMenu(0);
    // Click reset button.
    testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!.click();
    assertAnimation([true, false]);

    await browserProxy.whenCalled('resetNotificationPermissionForOrigins');

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', mockData);
    assertAnimation([false, false]);
  });

  /**
   * Tests whether clicking the Block All button will block notifications for
   * all entries in the list, and whether clicking the Undo button afterwards
   * will allow the notifications for that same list.
   */
  test('Block All Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#blockAllButton')!.click();
    const origins1 =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(2, origins1.length);
    assertEquals(
        JSON.stringify(origins1.sort()), JSON.stringify([origin1, origin2]));
    const notificationText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewBlockAllToastLabel', 2);
    assertNotification(true, notificationText);

    // Click undo button.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#undoToast cr-button')!.click();
    const origins2 =
        await browserProxy.whenCalled('allowNotificationPermissionForOrigins');
    assertEquals(2, origins2.length);
    assertEquals(
        JSON.stringify(origins2.sort()), JSON.stringify([origin1, origin2]));
  });

  test('Block All Click single entry', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', [{
          origin: origin1,
          notificationInfoString: detail1,
        }]);
    await flushTasks();

    const entries = getEntries();
    assertEquals(1, entries.length);

    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#blockAllButton')!.click();

    const blockedOrigins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(blockedOrigins[0], origin1);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            origin1));
  });

  test('Completion State', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    // Before review, header and list of permissions are visible.
    assertTrue(isChildVisible(testElement, '#review-header'));
    assertTrue(isChildVisible(testElement, '.notification-permissions-list'));
    assertFalse(isChildVisible(testElement, '#done-header'));

    // Through reviewing permissions the permission list is empty and only the
    // completion info is visible.
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', []);
    await flushTasks();
    assertFalse(isChildVisible(testElement, '#review-header'));
    assertFalse(isChildVisible(testElement, '.notification-permissions-list'));
    assertTrue(isChildVisible(testElement, '#done-header'));

    // The element returns to showing the list of permissions when new items are
    // added while the completion state is visible.
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', mockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, '#review-header'));
    assertTrue(isChildVisible(testElement, '.notification-permissions-list'));
    assertFalse(isChildVisible(testElement, '#done-header'));
  });

  test('Collapsible List', async function() {
    const expandButton =
        testElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);

    const notificationPermissionList =
        testElement.shadowRoot!.querySelector('iron-collapse');
    assertTrue(!!notificationPermissionList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(notificationPermissionList.opened);

    // User collapses the list.
    expandButton.click();
    flush();

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(notificationPermissionList.opened);

    // User expands the list.
    expandButton.click();
    flush();

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(notificationPermissionList.opened);
  });

  /**
   * TODO(crbug/1374908): Re-enable the commented parts. This is failing on
   * buildbots.
   *
   * Tests whether header string updated based on the notification permission
   * list size for plural and singular case.
   */
  test('Header String', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    await flushTasks();

    // Check header string for plural case.
    let entries = getEntries();
    assertEquals(2, entries.length);

    const headerElement =
        testElement.shadowRoot!.querySelector('#expandButton h2');
    assertTrue(headerElement !== null);
    // assertEquals(
    //     'Review 2 sites that recently sent a lot of notifications',
    //     headerElement.textContent!.trim());

    // Check header string for singular case.
    webUIListenerCallback(
        'notification-permission-review-list-maybe-changed', [{
          origin: origin1,
          notificationInfoString: detail1,
        }]);
    await flushTasks();

    entries = getEntries();
    assertEquals(1, entries.length);

    // assertEquals(
    //     'Review 1 site that recently sent a lot of notifications',
    //     headerElement.textContent!.trim());
  });
});
