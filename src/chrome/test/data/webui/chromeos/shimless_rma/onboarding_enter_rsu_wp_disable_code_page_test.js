// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {fakeRsuChallengeQrCode} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingEnterRsuWpDisableCodePage} from 'chrome://shimless-rma/onboarding_enter_rsu_wp_disable_code_page.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from '../../chai_assert.js';


/**
 * It is not possible to suppress visibility inline so this helper
 * function wraps the access to canvasSize_.
 * @suppress {visibility}
 */
function suppressedComponentCanvasSize_(component) {
  return component.canvasSize_;
}

export function onboardingEnterRsuWpDisableCodePageTest() {
  /** @type {?OnboardingEnterRsuWpDisableCodePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @param {string} challenge
   * @param {string} hwid
   * @return {!Promise}
   */
  function initializeEnterRsuWpDisableCodePage(challenge, hwid) {
    assertFalse(!!component);

    // Initialize the fake data.
    service.setGetRsuDisableWriteProtectChallengeResult(challenge);
    service.setGetRsuDisableWriteProtectHwidResult(hwid);
    service.setGetRsuDisableWriteProtectChallengeQrCodeResponse(
        fakeRsuChallengeQrCode);

    component = /** @type {!OnboardingEnterRsuWpDisableCodePage} */ (
        document.createElement('onboarding-enter-rsu-wp-disable-code-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @param {string} inputSelector
   * @return {!Promise}
   */
  function pressEnter(inputSelector) {
    const rsuCodeInput = component.shadowRoot.querySelector(inputSelector);
    rsuCodeInput.value = '12345678';
    rsuCodeInput.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    return flushTasks();
  }

  test('EnterRsuWpDisableCodePageInitializes', async () => {
    await initializeEnterRsuWpDisableCodePage('rsu challenge', '');
    const rsuCodeComponent = component.shadowRoot.querySelector('#rsuCode');
    assertFalse(rsuCodeComponent.hidden);
  });

  test('EnterRsuWpDisableCodePageRendersQrCode', async () => {
    await initializeEnterRsuWpDisableCodePage('', '');

    const expectedCanvasSize = 20;


    assertEquals(suppressedComponentCanvasSize_(component), expectedCanvasSize);
    const canvas = component.shadowRoot.querySelector('#qrCodeCanvas');
    assertTrue(!!canvas);
    assertEquals(canvas.width, expectedCanvasSize);
    assertEquals(canvas.height, expectedCanvasSize);

    const context = canvas.getContext('2d');
    assertTrue(!!context);
  });

  test(
      'EnterRsuWpDisableCodePageSetCodeOnNextCallsSetRsuDisableWriteProtectCode',
      async () => {
        const resolver = new PromiseResolver();
        await initializeEnterRsuWpDisableCodePage('', '');
        const expectedCode = 'RSU CODE';
        let savedCode = '';
        service.setRsuDisableWriteProtectCode = (code) => {
          savedCode = code;
          return resolver.promise;
        };
        const rsuCodeComponent = component.shadowRoot.querySelector('#rsuCode');
        rsuCodeComponent.value = expectedCode.toLowerCase();

        const expectedResult = {foo: 'bar'};
        let savedResult;
        component.onNextButtonClick().then((result) => savedResult = result);
        // Resolve to a distinct result to confirm it was not modified.
        resolver.resolve(expectedResult);
        await flushTasks();

        assertDeepEquals(savedCode, expectedCode);
        assertDeepEquals(savedResult, expectedResult);
      });

  test('EnterRsuWpDisableCodePageOpenChallengeDialog', async () => {
    await initializeEnterRsuWpDisableCodePage('', '');

    component.shadowRoot.querySelector('#rsuCodeDialogLink').click();
    assertTrue(component.shadowRoot.querySelector('#rsuChallengeDialog').open);
  });

  test('EnterRsuWpDisableCodePageDisableInput', async () => {
    await initializeEnterRsuWpDisableCodePage('', '');

    const rsuCodeInput = component.shadowRoot.querySelector('#rsuCode');
    assertFalse(rsuCodeInput.disabled);
    component.allButtonsDisabled = true;
    assertTrue(rsuCodeInput.disabled);
  });

  test('EnterRsuWpDisableCodePageStopChallengeDialogOpening', async () => {
    await initializeEnterRsuWpDisableCodePage('', '');

    component.allButtonsDisabled = true;
    component.shadowRoot.querySelector('#rsuCodeDialogLink').click();
    assertFalse(component.shadowRoot.querySelector('#rsuChallengeDialog').open);
  });

  test('EnterRsuWpDisableCodeRejectWrongCodeLength', async () => {
    await initializeEnterRsuWpDisableCodePage('', '');

    const rsuCodeInput = component.shadowRoot.querySelector('#rsuCode');

    // The code is shorter than the expected 8 characters.
    rsuCodeInput.value = '12345';
    assertFalse(rsuCodeInput.invalid);

    let wasPromiseRejected = false;
    component.onNextButtonClick()
        .then(
            () => assertNotReached(
                'RSU code should not be set with invalid code'))
        .catch(() => {
          wasPromiseRejected = true;
        });


    await flushTasks();
    assertTrue(wasPromiseRejected);
    assertTrue(rsuCodeInput.invalid);

    // Change the code so the invalid goes away.
    rsuCodeInput.value = '123';

    await flushTasks();
    assertFalse(rsuCodeInput.invalid);
  });

  test('EnterRsuWpDisableCodePagePressEnterkey', async () => {
    await initializeEnterRsuWpDisableCodePage(
        /*challenge=*/ '', /*hwid=*/ '');

    let nextButtonEventFired = false;
    component.addEventListener('click-next-button', (e) => {
      nextButtonEventFired = true;
    });
    await pressEnter('#rsuCode');

    assertTrue(nextButtonEventFired);
  });
}
