// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {beforeNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MinimumRoutes, Route, Router} from '../router.js';

import {ensureLazyLoaded} from './ensure_lazy_loaded.js';

/**
 * @enum {string}
 * A categorization of every possible Settings URL, necessary for implementing
 * a finite state machine.
 */
export const RouteState = {
  // Initial state before anything has loaded yet.
  INITIAL: 'initial',
  // A dialog that has a dedicated URL (e.g. /importData).
  DIALOG: 'dialog',
  // A section (basically a scroll position within the top level page, e.g,
  // /appearance.
  SECTION: 'section',
  // A subpage, or sub-subpage e.g, /searchEngins.
  SUBPAGE: 'subpage',
  // The top level Settings page, '/'.
  TOP_LEVEL: 'top-level',
};

/**
 * @param {?Route=} route
 * @return {!RouteState}
 */
function classifyRoute(route) {
  if (!route) {
    return RouteState.INITIAL;
  }
  const routes =
      /** @type {!MinimumRoutes} */ (Router.getInstance().getRoutes());
  if (route === routes.BASIC || route === routes.ABOUT) {
    return RouteState.TOP_LEVEL;
  }
  if (route.isSubpage()) {
    return RouteState.SUBPAGE;
  }
  if (route.isNavigableDialog) {
    return RouteState.DIALOG;
  }
  return RouteState.SECTION;
}

/**
 * Responds to route changes by expanding, collapsing, or scrolling to
 * sections on the page. Expanded sections take up the full height of the
 * container. At most one section should be expanded at any given time.
 * @polymerBehavior
 */
export const MainPageBehavior = {
  /** @type {?HTMLElement} */
  scroller: null,

  /**
   * A map holding all valid state transitions.
   * @private {!Map<!RouteState, !RouteState>}
   */
  validTransitions_: (function() {
    const allStates = new Set([
      RouteState.DIALOG,
      RouteState.SECTION,
      RouteState.SUBPAGE,
      RouteState.TOP_LEVEL,
    ]);

    return new Map([
      [RouteState.INITIAL, allStates],
      [
        RouteState.DIALOG,
        new Set([
          RouteState.SECTION,
          RouteState.SUBPAGE,
          RouteState.TOP_LEVEL,
        ]),
      ],
      [RouteState.SECTION, allStates],
      [RouteState.SUBPAGE, allStates],
      [RouteState.TOP_LEVEL, allStates],
    ]);
  })(),

  /** @override */
  attached() {
    this.scroller = this.domHost ? this.domHost.parentNode : document.body;
  },

  /**
   * Method to be defined by users of MainPageBehavior.
   * @param {!Route} route
   * @return {boolean} Whether the given route is part of |this| page.
   */
  containsRoute(route) {
    return false;
  },

  /**
   * @param {!Route} route
   * @return {boolean}
   * @private
   */
  shouldExpandAdvanced_(route) {
    const routes =
        /** @type {!MinimumRoutes} */ (Router.getInstance().getRoutes());
    return (this.tagName === 'OS-SETTINGS-PAGE') && routes.ADVANCED &&
        routes.ADVANCED.contains(route);
  },

  /**
   * Finds the settings section corresponding to the given route. If the
   * section is lazily loaded it force-renders it.
   * Note: If the section resides within "advanced" settings, a
   * 'hide-container' event is fired (necessary to avoid flashing). Callers
   * are responsible for firing a 'show-container' event.
   * @param {!Route} route
   * @return {!Promise<!HTMLElement>}
   * @private
   */
  ensureSectionForRoute_(route) {
    const section = this.getSection(route.section);
    if (section !== null) {
      return Promise.resolve(section);
    }

    // The function to use to wait for <dom-if>s to render.
    const waitFn = beforeNextRender.bind(null, this);

    return new Promise(resolve => {
      if (this.shouldExpandAdvanced_(route)) {
        this.fire('hide-container');
        waitFn(() => {
          this.$$('#advancedPageTemplate').get().then(() => {
            resolve(this.getSection(route.section));
          });
        });
      } else {
        waitFn(() => {
          resolve(this.getSection(route.section));
        });
      }
    });
  },

  /**
   * @param {!Route} route
   * @private
   */
  enterSubpage_(route) {
    this.lastScrollTop_ = this.scroller.scrollTop;
    this.scroller.scrollTop = 0;
    this.classList.add('showing-subpage');
    this.fire('subpage-expand');

    // Explicitly load the lazy_load.html module, since all subpages reside in
    // the lazy loaded module.

    // Polymer 3 codepath, do not delete next line comment.
    ensureLazyLoaded();

    this.ensureSectionForRoute_(route).then(section => {
      section.classList.add('expanded');
      // Fire event used by a11y tests only.
      this.fire('settings-section-expanded');

      this.fire('show-container');
    });
  },

  /**
   * @param {!Route} oldRoute
   * @return {!Promise<void>}
   * @private
   */
  enterMainPage_(oldRoute) {
    const oldSection = this.getSection(oldRoute.section);
    oldSection.classList.remove('expanded');
    this.classList.remove('showing-subpage');
    return new Promise((res, rej) => {
      requestAnimationFrame(() => {
        if (Router.getInstance().lastRouteChangeWasPopstate()) {
          this.scroller.scrollTop = this.lastScrollTop_;
        }
        this.fire('showing-main-page');
        res();
      });
    });
  },

  /**
   * @param {!Route} route
   * @private
   */
  scrollToSection_(route) {
    this.ensureSectionForRoute_(route).then(section => {
      this.fire('showing-section', section);
      this.fire('show-container');
    });
  },

  /**
   * Detects which state transition is appropriate for the given new/old
   * routes.
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @private
   */
  getStateTransition_(newRoute, oldRoute) {
    const containsNew = this.containsRoute(newRoute);
    const containsOld = this.containsRoute(oldRoute);

    if (!containsNew && !containsOld) {
      // Nothing to do, since none of the old/new routes belong to this page.
      return null;
    }

    // Case where going from |this| page to an unrelated page. For example:
    //  |this| is os-settings-page AND
    //  oldRoute is /searchEngines AND
    //  newRoute is /help.
    if (containsOld && !containsNew) {
      return [classifyRoute(oldRoute), RouteState.TOP_LEVEL];
    }

    // Case where return from an unrelated page to |this| page. For example:
    //  |this| is os-settings-page AND
    //  oldRoute is /help AND
    //  newRoute is /searchEngines
    if (!containsOld && containsNew) {
      return [RouteState.TOP_LEVEL, classifyRoute(newRoute)];
    }

    // Case where transitioning between routes that both belong to |this|
    // page.
    return [classifyRoute(oldRoute), classifyRoute(newRoute)];
  },

  /**
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    const transition = this.getStateTransition_(newRoute, oldRoute);
    if (transition === null) {
      return;
    }

    const oldState = transition[0];
    const newState = transition[1];
    assert(this.validTransitions_.get(oldState).has(newState));

    if (oldState === RouteState.TOP_LEVEL) {
      if (newState === RouteState.SECTION) {
        this.scrollToSection_(newRoute);
      } else if (newState === RouteState.SUBPAGE) {
        this.enterSubpage_(newRoute);
      }
      // Nothing to do here for the case of RouteState.DIALOG or TOP_LEVEL.
      // The latter happens when navigating from '/?search=foo' to '/'
      // (clearing search results).
      return;
    }

    if (oldState === RouteState.SECTION) {
      if (newState === RouteState.SECTION) {
        this.scrollToSection_(newRoute);
      } else if (newState === RouteState.SUBPAGE) {
        this.enterSubpage_(newRoute);
      } else if (newState === RouteState.TOP_LEVEL) {
        this.scroller.scrollTop = 0;
      }
      // Nothing to do here for the case of RouteState.DIALOG.
      return;
    }

    if (oldState === RouteState.SUBPAGE) {
      if (newState === RouteState.SECTION) {
        this.enterMainPage_(oldRoute);

        // Scroll to the corresponding section, only if the user explicitly
        // navigated to a section (via the menu).
        if (!Router.getInstance().lastRouteChangeWasPopstate()) {
          this.scrollToSection_(newRoute);
        }
      } else if (newState === RouteState.SUBPAGE) {
        // Handle case where the two subpages belong to
        // different sections, but are linked to each other. For example
        // /storage and /accounts (in ChromeOS).
        if (!oldRoute.contains(newRoute) && !newRoute.contains(oldRoute)) {
          this.enterMainPage_(oldRoute).then(() => {
            this.enterSubpage_(newRoute);
          });
          return;
        }

        // Handle case of subpage to sub-subpage navigation.
        if (oldRoute.contains(newRoute)) {
          this.scroller.scrollTop = 0;
          return;
        }
        // When going from a sub-subpage to its parent subpage, scroll
        // position is automatically restored, because we focus the
        // sub-subpage entry point.
      } else if (newState === RouteState.TOP_LEVEL) {
        this.enterMainPage_(oldRoute);
      } else if (newState === RouteState.DIALOG) {
        // The only known case currently for such a transition is from
        // /storage to /clearBrowserData.
        this.enterMainPage_(oldRoute);
      }
      return;
    }

    if (oldState === RouteState.INITIAL) {
      if (newState === RouteState.SECTION) {
        this.scrollToSection_(newRoute);
      } else if (newState === RouteState.SUBPAGE) {
        this.enterSubpage_(newRoute);
      }
      // Nothing to do here for the case of RouteState.DIALOG and TOP_LEVEL.
      return;
    }

    if (oldState === RouteState.DIALOG) {
      if (newState === RouteState.SUBPAGE) {
        // The only known case currently for such a transition is from
        // /clearBrowserData back to /storage.
        this.enterSubpage_(newRoute);
      }
      // Nothing to do for all other cases.
    }
  },

  /**
   * TODO(dpapad): Rename this to |querySection| to distinguish it from
   * ensureSectionForRoute_() which force-renders the section as needed.
   * Helper function to get a section from the local DOM.
   * @param {string} section Section name of the element to get.
   * @return {?HTMLElement}
   */
  getSection(section) {
    if (!section) {
      return null;
    }
    return /** @type {?HTMLElement} */ (
        this.$$(`settings-section[section="${section}"]`));
  },
};

/** @interface */
export class MainPageBehaviorInterface {
  constructor() {
    /** @type {?HTMLElement} */
    this.scroller;
  }

  /**
   * @param {!Route} route
   * @return {boolean} Whether the given route is part of |this| page.
   */
  containsRoute(route) {}

  /**
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {}

  /**
   * @param {string} section Section name of the element to get.
   * @return {?HTMLElement}
   */
  getSection(section) {}
}
