// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../select_to_speak/select_to_speak_e2e_test_base.js']);

/** Test fixture for automation_predicate.js. */
AutomationPredicateTest = class extends SelectToSpeakE2ETest {
  /**@override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'AutomationPredicate', '/common/automation_predicate.js');
  }
};

AX_TEST_F('AutomationPredicateTest', 'EquivalentRoles', async function() {
  const site = `
    <input type="text"></input>
    <input role="combobox"></input>
  `;
  const root = await this.runWithLoadedTree(site);
  // Text field is equivalent to text field with combo box.
  const textField = root.find({role: chrome.automation.RoleType.TEXT_FIELD});
  assertTrue(Boolean(textField), 'No text field found.');
  const textFieldWithComboBox =
      root.find({role: chrome.automation.RoleType.TEXT_FIELD_WITH_COMBO_BOX});
  assertTrue(
      Boolean(textFieldWithComboBox), 'No text field with combo box found.');

  // Gather all potential predicate names.
  const keys = Object.getOwnPropertyNames(AutomationPredicate);
  for (const key of keys) {
    // Not all keys are functions or predicates e.g. makeTableCellPredicate.
    if (typeof (AutomationPredicate[key]) !== 'function' ||
        key.indexOf('make') === 0) {
      continue;
    }

    const predicate = AutomationPredicate[key];
    if (predicate(textField)) {
      assertTrue(
          Boolean(predicate(textFieldWithComboBox)),
          `Textfield with combo box should match predicate ${key}`);
    }
  }
});
