// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import './date_time_types.js';
import './timezone_selector.js';
import './timezone_subpage.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getTemplate} from './date_time_page.html.js';
import {TimeZoneBrowserProxy, TimeZoneBrowserProxyImpl} from './timezone_browser_proxy.js';

const SettingsDateTimePageElementBase =
    mixinBehaviors(
        [
          DeepLinkingBehavior,
          PrefsBehavior,
          RouteObserverBehavior,
        ],
        I18nMixin(WebUIListenerMixin(PolymerElement))) as {
      new (): PolymerElement & DeepLinkingBehaviorInterface &
          PrefsBehaviorInterface & RouteObserverBehaviorInterface &
          I18nMixinInterface & WebUIListenerMixinInterface,
    };

class SettingsDateTimePageElement extends SettingsDateTimePageElementBase {
  static get is() {
    return 'settings-date-time-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether date and time are settable. Normally the date and time are
       * forced by network time, so default to false to initially hide the
       * button.
       */
      canSetDateTime_: {
        type: Boolean,
        value: false,
      },

      /**
       * This is used to get current time zone display name from
       * <timezone-selector> via bi-directional binding.
       */
      activeTimeZoneDisplayName: {
        type: String,
        value: loadTimeData.getString('timeZoneName'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.DATETIME_TIMEZONE_SUBPAGE) {
            map.set(
                routes.DATETIME_TIMEZONE_SUBPAGE.path,
                '#timeZoneSettingsTrigger');
          }
          return map;
        },
      },

      timeZoneSettingSubLabel_: {
        type: String,
        computed: `computeTimeZoneSettingSubLabel_(
            activeTimeZoneDisplayName,
            prefs.generated.resolve_timezone_by_geolocation_on_off.value,
            prefs.generated.resolve_timezone_by_geolocation_method_short.value)`,
      },

      /**
       * Whether the icon informing that this action is managed by a parent is
       * displayed.
       */
      displayManagedByParentIcon_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isChild'),
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.k24HourClock,
          Setting.kChangeTimeZone,
        ]),
      },

    };
  }

  activeTimeZoneDisplayName: string;
  private browserProxy_: TimeZoneBrowserProxy;
  private canSetDateTime_: boolean;
  private displayManagedByParentIcon_: boolean;
  private focusConfig_: Map<string, string>;
  private timeZoneSettingSubLabel_: string;

  constructor() {
    super();

    this.browserProxy_ = TimeZoneBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));
    this.browserProxy_.dateTimePageReady();
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.DATETIME) {
      return;
    }

    this.attemptDeepLink();
  }

  private onCanSetDateTimeChanged_(canSetDateTime: boolean) {
    this.canSetDateTime_ = canSetDateTime;
  }

  private onSetDateTimeTap_() {
    this.browserProxy_.showSetDateTimeUI();
  }

  private computeTimeZoneSettingSubLabel_(): string {
    if (!this.getPref('generated.resolve_timezone_by_geolocation_on_off')
             .value) {
      return this.activeTimeZoneDisplayName;
    }
    const method =
        this.getPref('generated.resolve_timezone_by_geolocation_method_short')
            .value as number;
    const id = [
      'setTimeZoneAutomaticallyDisabled',
      'setTimeZoneAutomaticallyIpOnlyDefault',
      'setTimeZoneAutomaticallyWithWiFiAccessPointsData',
      'setTimeZoneAutomaticallyWithAllLocationInfo',
    ][method];
    return id ? this.i18n(id) : '';
  }

  private onTimeZoneSettings_() {
    this.openTimeZoneSubpage_();
  }

  private openTimeZoneSubpage_() {
    Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-date-time-page': SettingsDateTimePageElement;
  }
}

customElements.define(
    SettingsDateTimePageElement.is, SettingsDateTimePageElement);
