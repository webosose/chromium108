// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl, SettingsCreditCardEditDialogElement, SettingsPaymentsSectionElement, SettingsVirtualCardUnenrollDialogElement} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';

import {createCreditCardEntry, createEmptyCreditCardEntry, PaymentsManagerExpectations,TestPaymentsManager} from './passwords_and_autofill_fake_data.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('PaymentSectionUiTest', function() {
  test('testAutofillExtensionIndicator', function() {
    // Initializing with fake prefs
    const section = document.createElement('settings-payments-section');
    section.prefs = {
      autofill: {credit_card_enabled: {}, credit_card_fido_auth_enabled: {}},
    };
    document.body.appendChild(section);

    assertFalse(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
    section.set('prefs.autofill.credit_card_enabled.extensionId', 'test-id-1');
    section.set(
        'prefs.autofill.credit_card_fido_auth_enabled.extensionId',
        'test-id-2');
    flush();

    assertTrue(
        !!section.shadowRoot!.querySelector('#autofillExtensionIndicator'));
  });
});

suite('PaymentsSection', function() {
  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    loadTimeData.overrideValues({
      migrationEnabled: true,
      virtualCardEnrollmentEnabled: true,
      virtualCardMetadataEnabled: true,
    });
  });

  /**
   * Creates the payments autofill section for the given list.
   * @param {!Object} prefValues
   */
  function createPaymentsSection(
      creditCards: chrome.autofillPrivate.CreditCardEntry[], upiIds: string[],
      prefValues: any): SettingsPaymentsSectionElement {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    paymentsManager.data.upiIds = upiIds;
    PaymentsManagerImpl.setInstance(paymentsManager);

    const section = document.createElement('settings-payments-section');
    section.prefs = {autofill: prefValues};
    document.body.appendChild(section);
    flush();

    return section;
  }

  /**
   * Creates the Edit Credit Card dialog.
   */
  function createCreditCardDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsCreditCardEditDialogElement {
    const section = document.createElement('settings-credit-card-edit-dialog');
    section.creditCard = creditCardItem;
    document.body.appendChild(section);
    flush();
    return section;
  }

  /**
   * Creates a virtual card unenroll dialog.
   */
  function createVirtualCardUnenrollDialog(
      creditCardItem: chrome.autofillPrivate.CreditCardEntry):
      SettingsVirtualCardUnenrollDialogElement {
    const dialog =
        document.createElement('settings-virtual-card-unenroll-dialog');
    dialog.creditCard = creditCardItem;
    document.body.appendChild(dialog);
    flush();
    return dialog;
  }

  // Fakes the existence of a platform authenticator.
  function addFakePlatformAuthenticator() {
    (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
        .setIsUserVerifyingPlatformAuthenticatorAvailable(true);
  }


  /**
   * Returns an array containing the local and server credit card items.
   */
  function getLocalAndServerCreditCardListItems() {
    return document.body.querySelector('settings-payments-section')!.shadowRoot!
        .querySelector('#paymentsList')!.shadowRoot!.querySelectorAll(
            'settings-credit-card-list-entry')!;
  }

  /**
   * Returns the shadow root of the card row from the specified list of
   * payment methods.
   */
  function getCardRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row = paymentsList.shadowRoot!.querySelector(
        'settings-credit-card-list-entry');
    assertTrue(!!row);
    return row!.shadowRoot!;
  }

  /**
   * Returns the shadow root of the UPI ID row from the specified list of
   * payment methods.
   */
  function getUPIRowShadowRoot(paymentsList: HTMLElement): ShadowRoot {
    const row =
        paymentsList.shadowRoot!.querySelector('settings-upi-id-list-entry');
    assertTrue(!!row);
    return row!.shadowRoot!;
  }

  /**
   * Returns the default expectations from TestPaymentsManager. Adjust the
   * values as needed.
   */
  function getDefaultExpectations(): PaymentsManagerExpectations {
    const expected = new PaymentsManagerExpectations();
    expected.requestedCreditCards = 1;
    expected.listeningCreditCards = 1;
    expected.removedCreditCards = 0;
    expected.clearedCachedCreditCards = 0;
    expected.addedVirtualCards = 0;
    return expected;
  }

  test('verifyNoCreditCards', function() {
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(0, getLocalAndServerCreditCardListItems().length);

    assertFalse(
        creditCardList.shadowRoot!
            .querySelector<HTMLElement>('#noPaymentMethodsLabel')!.hidden);
    assertTrue(creditCardList.shadowRoot!
                   .querySelector<HTMLElement>('#creditCardsHeading')!.hidden);
    assertFalse(section.$.autofillCreditCardToggle.disabled);
    assertFalse(section.$.addCreditCard.disabled);
  });

  test('verifyCreditCardsDisabled', function() {
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: false}});

    assertFalse(section.$.autofillCreditCardToggle.disabled);
    assertTrue(section.$.addCreditCard.hidden);
  });

  test('verifyCreditCardCount', function() {
    const creditCards = [
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
      createCreditCardEntry(),
    ];

    const section = createPaymentsSection(
        creditCards, /*upiIds=*/[], {credit_card_enabled: {value: true}});
    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(
        creditCards.length, getLocalAndServerCreditCardListItems().length);

    assertTrue(
        creditCardList.shadowRoot!
            .querySelector<HTMLElement>('#noPaymentMethodsLabel')!.hidden);
    assertFalse(creditCardList.shadowRoot!
                    .querySelector<HTMLElement>('#creditCardsHeading')!.hidden);
    assertFalse(section.$.autofillCreditCardToggle.disabled);
    assertFalse(section.$.addCreditCard.disabled);
  });

  test('verifyCreditCardFields', function() {
    const creditCard = createCreditCardEntry();
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertEquals(
        creditCard.metadata!.summaryLabel,
        rowShadowRoot.querySelector<HTMLElement>(
                         '#summaryLabel')!.textContent!.trim());
    assertEquals(
        creditCard.expirationMonth + '/' + creditCard.expirationYear,
        rowShadowRoot.querySelector<HTMLElement>(
                         '#creditCardExpiration')!.textContent!.trim());
  });

  test('verifyCreditCardRowButtonIsDropdownWhenLocal', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertFalse(!!outlinkButton);
  });

  test('verifyCreditCardMoreDetailsTitle', function() {
    let creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = true;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    const updateCreditCardCallback =
        (creditCard: chrome.autofillPrivate.CreditCardEntry) => {
          (PaymentsManagerImpl.getInstance() as TestPaymentsManager)
              .lastCallback.setPersonalDataManagerListener!([], [creditCard]);
          flush();
        };

    // Case 1: a card with a nickname
    creditCard = createCreditCardEntry();
    creditCard.nickname = 'My card name';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for My card name', menuButton!.getAttribute('title'));

    // Case 2: a card without nickname
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = 'Visa';
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Visa ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 3: a card without network
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = '0000000000001234';
    creditCard.network = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Card ending in 1234',
        menuButton!.getAttribute('title'));

    // Case 4: a card without number
    creditCard = createCreditCardEntry();
    creditCard.cardNumber = undefined;
    updateCreditCardCallback(creditCard);
    assertEquals(
        'More actions for Jane Doe', menuButton!.getAttribute('title'));
  });

  test('verifyCreditCardRowButtonIsOutlinkWhenRemote', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertFalse(!!menuButton);
    const outlinkButton =
        rowShadowRoot.querySelector('cr-icon-button.icon-external');
    assertTrue(!!outlinkButton);
  });

  test(
      'verifyCreditCardRowButtonIsDropdownWhenVirtualCardEnrollEligible',
      function() {
        const creditCard = createCreditCardEntry();
        creditCard.metadata!.isLocal = false;
        creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
        creditCard.metadata!.isVirtualCardEnrolled = false;
        const section = createPaymentsSection(
            [creditCard], /*upiIds=*/[], /*prefValues=*/ {});
        const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
        const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
        assertTrue(!!menuButton);
        const outlinkButton =
            rowShadowRoot.querySelector('cr-icon-button.icon-external');
        assertFalse(!!outlinkButton);
      });

  test('verifyCreditCardSummarySublabelWhenSublabelIsValid', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
  });

  test('verifyCreditCardSummarySublabelWhenSublabelIsInvalid', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    creditCard.metadata!.summarySublabel = '';
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#summarySublabel')!.hidden);
  });

  test('verifyCreditCardSummarySublabelWhenVirtualCardAvailable', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
    assertEquals(
        'Virtual card available',
        getCardRowShadowRoot(section.$.paymentsList)
            .querySelector<HTMLElement>(
                '#summarySublabel')!.textContent!.trim());
  });

  test('verifyCreditCardSummarySublabelWhenVirtualCardTurnedOn', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = true;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#summarySublabel')!.hidden);
    assertEquals(
        'Virtual card turned on',
        getCardRowShadowRoot(section.$.paymentsList)
            .querySelector<HTMLElement>(
                '#summarySublabel')!.textContent!.trim());
  });

  test('verifyPaymentsLabel', function() {
    loadTimeData.overrideValues({
      virtualCardMetadataEnabled: false,
    });
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#paymentsLabel')!.hidden);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#paymentsIndicator')!.hidden);
  });

  test('verifyPaymentsIndicator', function() {
    loadTimeData.overrideValues({
      virtualCardMetadataEnabled: true,
    });
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertTrue(getCardRowShadowRoot(section.$.paymentsList)
                   .querySelector<HTMLElement>('#paymentsLabel')!.hidden);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#paymentsIndicator')!.hidden);
  });

  test('verifyCardImage', function() {
    loadTimeData.overrideValues({
      virtualCardMetadataEnabled: true,
    });
    const creditCard = createCreditCardEntry();
    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});

    const creditCardList = section.$.paymentsList;
    assertTrue(!!creditCardList);
    assertEquals(1, getLocalAndServerCreditCardListItems().length);
    assertFalse(getCardRowShadowRoot(section.$.paymentsList)
                    .querySelector<HTMLElement>('#cardImage')!.hidden);
  });

  test('verifyAddVsEditCreditCardTitle', function() {
    const newCreditCard = createEmptyCreditCardEntry();
    const newCreditCardDialog = createCreditCardDialog(newCreditCard);
    const oldCreditCard = createCreditCardEntry();
    const oldCreditCardDialog = createCreditCardDialog(oldCreditCard);

    function getTitle(dialog: SettingsCreditCardEditDialogElement): string {
      return dialog.shadowRoot!.querySelector('[slot=title]')!.textContent!;
    }

    const oldTitle = getTitle(oldCreditCardDialog);
    const newTitle = getTitle(newCreditCardDialog);
    assertNotEquals(oldTitle, newTitle);
    assertNotEquals('', oldTitle);
    assertNotEquals('', newTitle);

    // Wait for dialogs to open before finishing test.
    return Promise.all([
      whenAttributeIs(newCreditCardDialog.$.dialog, 'open', ''),
      whenAttributeIs(oldCreditCardDialog.$.dialog, 'open', ''),
    ]);
  });

  test('verifyExpiredCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // 2015 is over unless time goes wobbly.
    const twentyFifteen = 2015;
    creditCard.expirationYear = twentyFifteen.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const now = new Date();
          const maxYear = now.getFullYear() + 19;
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals('2015', yearOptions[0]!.textContent!.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
        });
  });

  test('verifyVeryFutureCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // Expiring 25 years from now is unusual.
    const now = new Date();
    const farFutureYear = now.getFullYear() + 25;
    creditCard.expirationYear = farFutureYear.toString();

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals(
              now.getFullYear().toString(),
              yearOptions[0]!.textContent!.trim());
          assertEquals(
              farFutureYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
        });
  });

  test('verifyVeryNormalCreditCardYear', function() {
    const creditCard = createCreditCardEntry();

    // Expiring 2 years from now is not unusual.
    const now = new Date();
    const nearFutureYear = now.getFullYear() + 2;
    creditCard.expirationYear = nearFutureYear.toString();
    const maxYear = now.getFullYear() + 19;

    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          const yearOptions = creditCardDialog.$.year.options;

          assertEquals(
              now.getFullYear().toString(),
              yearOptions[0]!.textContent!.trim());
          assertEquals(
              maxYear.toString(),
              yearOptions[yearOptions.length - 1]!.textContent!.trim());
          assertEquals(
              creditCard.expirationYear, creditCardDialog.$.year.value);
        });
  });

  test('verify save new credit card', function() {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    return whenAttributeIs(creditCardDialog.$.dialog, 'open', '')
        .then(function() {
          // Not expired, but still can't be saved, because there's no
          // name.
          const expiredError = creditCardDialog.$.expiredError;
          assertEquals('hidden', getComputedStyle(expiredError).visibility);
          assertTrue(creditCardDialog.$.saveButton.disabled);

          // Add a name.
          creditCardDialog.set('name_', 'Jane Doe');
          flush();

          assertEquals('hidden', getComputedStyle(expiredError).visibility);
          assertFalse(creditCardDialog.$.saveButton.disabled);

          const savedPromise =
              eventToPromise('save-credit-card', creditCardDialog);
          creditCardDialog.$.saveButton.click();
          return savedPromise;
        })
        .then(function(event) {
          assertEquals(creditCard.guid, event.detail.guid);
        });
  });

  test('verifyNotEditedEntryAfterCancel', async function() {
    const creditCard = createCreditCardEntry();
    let creditCardDialog = createCreditCardDialog(creditCard);

    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    // Edit a entry.
    creditCardDialog.set('name_', 'EditedName');
    creditCardDialog.set('nickname_', 'NickName');
    creditCardDialog.set('cardNumber_', '0000000000001234');
    flush();

    creditCardDialog.$.cancelButton.click();
    await eventToPromise('close', creditCardDialog);

    creditCardDialog = createCreditCardDialog(creditCard);
    await whenAttributeIs(creditCardDialog.$.dialog, 'open', '');

    assertEquals(creditCardDialog.get('name_'), creditCard.name);
    assertEquals(creditCardDialog.get('cardNumber_'), creditCard.cardNumber);
    assertEquals(creditCardDialog.get('nickname_'), creditCard.nickname);
  });

  test('verifyCancelCreditCardEdit', function(done) {
    const creditCard = createEmptyCreditCardEntry();
    const creditCardDialog = createCreditCardDialog(creditCard);

    whenAttributeIs(creditCardDialog.$.dialog, 'open', '').then(function() {
      eventToPromise('save-credit-card', creditCardDialog).then(function() {
        // Fail the test because the save event should not be called
        // when cancel is clicked.
        assertTrue(false);
      });

      eventToPromise('close', creditCardDialog).then(function() {
        // Test is |done| in a timeout in order to ensure that
        // 'save-credit-card' is NOT fired after this test.
        window.setTimeout(done, 100);
      });

      creditCardDialog.$.cancelButton.click();
    });
  });

  test('verifyLocalCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    // When credit card is local, |isCached| will be undefined.
    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isCached = undefined;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Local credit cards will show the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertFalse(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyCachedCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Cached remote CCs will show overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertFalse(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyNotCachedCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // No overflow menu when not cached.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    assertFalse(!!rowShadowRoot.querySelector('#creditCardMenu'));
  });

  test('verifyVirtualCardEligibleCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertFalse(section.$.menuAddVirtualCard.hidden);
    assertTrue(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyVirtualCardEnrolledCreditCardMenu', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    // Server cards that are eligible for virtual card enrollment should show
    // the overflow menu.
    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);

    menuButton.click();
    flush();

    // Menu should have 2 options.
    assertFalse(section.$.menuEditCreditCard.hidden);
    assertTrue(section.$.menuRemoveCreditCard.hidden);
    assertTrue(section.$.menuClearCreditCard.hidden);
    assertTrue(section.$.menuAddVirtualCard.hidden);
    assertFalse(section.$.menuRemoveVirtualCard.hidden);

    section.$.creditCardSharedMenu.close();
    flush();
  });

  test('verifyClearCachedCreditCardClicked', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = true;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuClearCreditCard.hidden);
    section.$.menuClearCreditCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.clearedCachedCreditCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveCreditCardClicked', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = true;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = false;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuRemoveCreditCard.hidden);
    section.$.menuRemoveCreditCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.removedCreditCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyAddVirtualCardClicked', function() {
    const creditCard = createCreditCardEntry();

    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = false;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuAddVirtualCard.hidden);
    section.$.menuAddVirtualCard.click();
    flush();

    const paymentsManager =
        PaymentsManagerImpl.getInstance() as TestPaymentsManager;
    const expectations = getDefaultExpectations();
    expectations.addedVirtualCards = 1;
    paymentsManager.assertExpectations(expectations);
  });

  test('verifyRemoveVirtualCardClicked', function() {
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isLocal = false;
    creditCard.metadata!.isCached = false;
    creditCard.metadata!.isVirtualCardEnrollmentEligible = true;
    creditCard.metadata!.isVirtualCardEnrolled = true;

    const section =
        createPaymentsSection([creditCard], /*upiIds=*/[], /*prefValues=*/ {});
    assertEquals(1, getLocalAndServerCreditCardListItems().length);

    const rowShadowRoot = getCardRowShadowRoot(section.$.paymentsList);
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    assertFalse(section.$.menuRemoveVirtualCard.hidden);
    section.$.menuRemoveVirtualCard.click();
    flush();

    const menu =
        rowShadowRoot.querySelector<HTMLElement>('#creditCardSharedMenu');
    assertFalse(!!menu);
  });

  test('verifyVirtualCardUnenrollDialogConfirmed', async function() {
    const creditCard = createCreditCardEntry();
    creditCard.guid = '12345';
    const dialog = createVirtualCardUnenrollDialog(creditCard);

    // Wait for the dialog to open.
    await whenAttributeIs(dialog.$.dialog, 'open', '');

    const promise = eventToPromise('unenroll-virtual-card', dialog);
    dialog.$.confirmButton.click();
    const event = await promise;
    assertEquals(event.detail, '12345');
  });

  test('verifyMigrationButtonNotShownIfMigrationNotEnabled', function() {
    // Mock prerequisites are not met.
    loadTimeData.overrideValues({migrationEnabled: false});

    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfCreditCardDisabled', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    // Mock credit card save toggle is turned off by users.
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: false}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonNotShownIfNoCardIsMigratable', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    // Mock credit card is not valid.
    creditCard.metadata!.isMigratable = false;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertTrue(section.$.migrateCreditCards.hidden);
  });

  test('verifyMigrationButtonShown', function() {
    // Add one migratable credit card.
    const creditCard = createCreditCardEntry();
    creditCard.metadata!.isMigratable = true;
    const section = createPaymentsSection(
        [creditCard], /*upiIds=*/[], {credit_card_enabled: {value: true}});

    assertFalse(section.$.migrateCreditCards.hidden);
  });

  test('verifyFIDOAuthToggleShownIfUserIsVerifiable', function() {
    // Set |fidoAuthenticationAvailableForAutofill| to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});

    assertTrue(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleNotShownIfUserIsNotVerifiable', function() {
    // Set |fidoAuthenticationAvailableForAutofill| to false.
    loadTimeData.overrideValues(
        {fidoAuthenticationAvailableForAutofill: false});
    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[],
        {credit_card_enabled: {value: true}});
    assertFalse(!!section.shadowRoot!.querySelector(
        '#autofillCreditCardFIDOAuthToggle'));
  });

  test('verifyFIDOAuthToggleCheckedIfOptedIn', function() {
    // Set FIDO auth pref value to true.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(/*creditCards=*/[], /*upiIds=*/[], {
      credit_card_enabled: {value: true},
      credit_card_fido_auth_enabled: {value: true},
    });
    assertTrue(section.shadowRoot!
                   .querySelector<SettingsToggleButtonElement>(
                       '#autofillCreditCardFIDOAuthToggle')!.checked);
  });

  test('verifyFIDOAuthToggleUncheckedIfOptedOut', function() {
    // Set FIDO auth pref value to false.
    loadTimeData.overrideValues({fidoAuthenticationAvailableForAutofill: true});
    addFakePlatformAuthenticator();
    const section = createPaymentsSection(/*creditCards=*/[], /*upiIds=*/[], {
      credit_card_enabled: {value: true},
      credit_card_fido_auth_enabled: {value: false},
    });
    assertFalse(section.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#autofillCreditCardFIDOAuthToggle')!.checked);
  });

  test('verifyUpiIdRow', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = createPaymentsSection(
        /*creditCards=*/[], ['vpa@indianbank'], /*prefValues=*/ {});
    const rowShadowRoot = getUPIRowShadowRoot(section.$.paymentsList);
    assertTrue(!!rowShadowRoot);
    assertEquals(
        rowShadowRoot.querySelector<HTMLElement>('#upiIdLabel')!.textContent,
        'vpa@indianbank');
  });

  test('verifyNoUpiId', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('verifyUpiIdCount', function() {
    loadTimeData.overrideValues({showUpiIdSettings: true});

    const upiIds = ['vpa1@indianbank', 'vpa2@indianbank'];
    const section = createPaymentsSection(
        /*creditCards=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(upiIds.length, upiRows.length);
  });

  // Test that |showUpiIdSettings| controls showing UPI IDs in the page.
  test('verifyShowUpiIdSettings', function() {
    loadTimeData.overrideValues({showUpiIdSettings: false});

    const upiIds = ['vpa1@indianbank'];
    const section = createPaymentsSection(
        /*creditCards=*/[], upiIds, /*prefValues=*/ {});

    const paymentsList = section.$.paymentsList;
    const upiRows =
        paymentsList.shadowRoot!.querySelectorAll('settings-upi-id-list-entry');

    assertEquals(0, upiRows.length);
  });

  test('CanMakePaymentToggle_RecordsMetrics', async function() {
    const testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const section = createPaymentsSection(
        /*creditCards=*/[], /*upiIds=*/[], /*prefValues=*/ {});

    section.$.canMakePaymentToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');

    assertEquals(PrivacyElementInteractions.PAYMENT_METHOD, result);
  });

  test('verifyVirtualCardUnenrollDialogContent', function() {
    const creditCard = createCreditCardEntry();
    const dialog = createVirtualCardUnenrollDialog(creditCard);

    const title = dialog.shadowRoot!.querySelector('[slot=title]')!;
    const body = dialog.shadowRoot!.querySelector('[slot=body]')!;
    assertNotEquals('', title.textContent);
    assertNotEquals('', body.textContent);

    // Wait for dialogs to open before finishing test.
    return whenAttributeIs(dialog.$.dialog, 'open', '');
  });
});
