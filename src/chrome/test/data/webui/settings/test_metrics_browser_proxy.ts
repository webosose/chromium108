// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsBrowserProxy, PrivacyElementInteractions, PrivacyGuideInteractions, PrivacyGuideSettingsStates, SafeBrowsingInteractions, SafetyCheckInteractions, SafetyCheckNotificationsModuleInteractions} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordSafetyCheckInteractionHistogram',
      'recordSafetyCheckNotificationsListCountHistogram',
      'recordSafetyCheckNotificationsModuleInteractionsHistogram',
      'recordSettingsPageHistogram',
      'recordSafeBrowsingInteractionHistogram',
      'recordPrivacyGuideNextNavigationHistogram',
      'recordPrivacyGuideEntryExitHistogram',
      'recordPrivacyGuideSettingsStatesHistogram',
    ]);
  }

  recordAction(action: string) {
    this.methodCalled('recordAction', action);
  }

  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions) {
    this.methodCalled('recordSafetyCheckInteractionHistogram', interaction);
  }

  recordSafetyCheckNotificationsListCountHistogram(suggestions: number) {
    this.methodCalled(
        'recordSafetyCheckNotificationsListCountHistogram', suggestions);
  }

  recordSafetyCheckNotificationsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    this.methodCalled(
        'recordSafetyCheckNotificationsModuleInteractionHistogram',
        interaction);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  recordSafeBrowsingInteractionHistogram(interaction:
                                             SafeBrowsingInteractions) {
    this.methodCalled('recordSafeBrowsingInteractionHistogram', interaction);
  }

  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideNextNavigationHistogram', interaction);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideEntryExitHistogram', interaction);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    this.methodCalled('recordPrivacyGuideSettingsStatesHistogram', state);
  }
}
