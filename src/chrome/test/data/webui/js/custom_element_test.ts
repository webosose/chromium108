// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

class TestElement extends CustomElement {
  static override get template() {
    return '<div id="content"></div>';
  }
}

customElements.define('test-element', TestElement);

let testElement: TestElement;

suite('CustomElementTest', function() {
  setup(function() {
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  test('Template', function() {
    assertEquals(TestElement.template, testElement.shadowRoot!.innerHTML);
  });

  test('Test $()', function() {
    assertTrue(
        testElement.$('#content') ===
        testElement.shadowRoot!.getElementById('content'));
  });

  test('Test $all()', function() {
    assertTrue(
        testElement.$all('#content')[0] ===
        testElement.shadowRoot!.getElementById('content'));
  });
});
