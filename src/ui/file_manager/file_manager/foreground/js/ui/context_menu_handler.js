// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.js';
import {EventTracker} from 'chrome://resources/ash/common/event_tracker.js';
import {isWindows, isLinux, isMac, isLacros, dispatchPropertyChange} from 'chrome://resources/js/cr.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {HideType} from './menu_button.js';
import {positionPopupAtPoint} from './position_util.js';
// clang-format on


  /**
   * Handles context menus.
   * @implements {EventListener}
   */
  class ContextMenuHandler extends EventTarget {
    constructor() {
      super();
      /** @private {!EventTracker} */
      this.showingEvents_ = new EventTracker();

      /**
       * The menu that we are currently showing.
       * @private {?Menu}
       */
      this.menu_ = null;

      /** @private {?number} */
      this.hideTimestamp_ = null;

      /** @private {boolean} */
      this.keyIsDown_ = false;
    }

    get menu() {
      return this.menu_;
    }

    /**
     * Shows a menu as a context menu.
     * @param {!Event} e The event triggering the show (usually a contextmenu
     *     event).
     * @param {!Menu} menu The menu to show.
     */
    showMenu(e, menu) {
      menu.updateCommands(assertInstanceof(e.currentTarget, Node));
      if (!menu.hasVisibleItems()) {
        return;
      }

      this.menu_ = menu;
      menu.classList.remove('hide-delayed');
      menu.show({x: e.screenX, y: e.screenY});
      menu.contextElement = e.currentTarget;

      // When the menu is shown we steal a lot of events.
      const doc = menu.ownerDocument;
      const win = /** @type {!Window} */ (doc.defaultView);
      this.showingEvents_.add(doc, 'keydown', this, true);
      this.showingEvents_.add(doc, 'mousedown', this, true);
      this.showingEvents_.add(doc, 'touchstart', this, true);
      this.showingEvents_.add(doc, 'focus', this);
      this.showingEvents_.add(win, 'popstate', this);
      this.showingEvents_.add(win, 'resize', this);
      this.showingEvents_.add(win, 'blur', this);
      this.showingEvents_.add(menu, 'contextmenu', this);
      this.showingEvents_.add(menu, 'activate', this);
      this.positionMenu_(e, menu);

      const ev = new Event('show');
      ev.element = menu.contextElement;
      ev.menu = menu;
      this.dispatchEvent(ev);
    }

    /**
     * Hide the currently shown menu.
     * @param {HideType=} opt_hideType Type of hide.
     *     default: HideType.INSTANT.
     */
    hideMenu(opt_hideType) {
      const menu = this.menu;
      if (!menu) {
        return;
      }

      if (opt_hideType === HideType.DELAYED) {
        menu.classList.add('hide-delayed');
      } else {
        menu.classList.remove('hide-delayed');
      }
      menu.hide();
      const originalContextElement = menu.contextElement;
      menu.contextElement = null;
      this.showingEvents_.removeAll();
      menu.selectedIndex = -1;
      this.menu_ = null;

      // On windows we might hide the menu in a right mouse button up and if
      // that is the case we wait some short period before we allow the menu
      // to be shown again.
      this.hideTimestamp_ = isWindows ? Date.now() : 0;

      const ev = new Event('hide');
      ev.element = originalContextElement;
      ev.menu = menu;
      this.dispatchEvent(ev);
    }

    /**
     * Positions the menu
     * @param {!Event} e The event object triggering the showing.
     * @param {!Menu} menu The menu to position.
     * @private
     */
    positionMenu_(e, menu) {
      // TODO(arv): Handle scrolled documents when needed.

      const element = e.currentTarget;
      let x;
      let y;
      // When the user presses the context menu key (on the keyboard) we need
      // to detect this.
      if (this.keyIsDown_) {
        const rect = element.getRectForContextMenu ?
            element.getRectForContextMenu() :
            element.getBoundingClientRect();
        const offset = Math.min(rect.width, rect.height) / 2;
        x = rect.left + offset;
        y = rect.top + offset;
      } else {
        x = e.clientX;
        y = e.clientY;
      }

      positionPopupAtPoint(x, y, menu);
    }

    /**
     * Handles event callbacks.
     * @param {!Event} e The event object.
     */
    handleEvent(e) {
      // Keep track of keydown state so that we can use that to determine the
      // reason for the contextmenu event.
      switch (e.type) {
        case 'keydown':
          this.keyIsDown_ = !e.ctrlKey && !e.altKey &&
              // context menu key or Shift-F10
              (e.keyCode === 93 && !e.shiftKey ||
               e.key === 'F10' && e.shiftKey);
          break;

        case 'keyup':
          this.keyIsDown_ = false;
          break;
      }

      // Context menu is handled even when we have no menu.
      if (e.type !== 'contextmenu' && !this.menu) {
        return;
      }

      switch (e.type) {
        case 'mousedown':
          if (!this.menu.contains(e.target)) {
            this.hideMenu();
            if (e.button === 0 /* Left button */ &&
                (isLinux || isMac || isLacros)) {
              // Emulate Mac and Linux, which swallow native 'mousedown' events
              // that close menus.
              e.preventDefault();
              e.stopPropagation();
            }
          } else {
            e.preventDefault();
          }
          break;

        case 'touchstart':
          if (!this.menu.contains(e.target)) {
            this.hideMenu();
          }
          break;

        case 'keydown':
          if (e.key === 'Escape') {
            this.hideMenu();
            e.stopPropagation();
            e.preventDefault();

            // If the menu is visible we let it handle all the keyboard events.
          } else if (this.menu) {
            this.menu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }
          break;

        case 'activate':
          const hideDelayed =
              e.target instanceof MenuItem && e.target.checkable;
          this.hideMenu(
              hideDelayed ? HideType.DELAYED : HideType.INSTANT);
          break;

        case 'focus':
          if (!this.menu.contains(e.target)) {
            this.hideMenu();
          }
          break;

        case 'blur':
          this.hideMenu();
          break;

        case 'popstate':
        case 'resize':
          this.hideMenu();
          break;

        case 'contextmenu':
          if ((!this.menu || !this.menu.contains(e.target)) &&
              (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50)) {
            this.showMenu(e, e.currentTarget.contextMenu);
          }
          e.preventDefault();
          // Don't allow elements further up in the DOM to show their menus.
          e.stopPropagation();
          break;
      }
    }

    /**
     * Adds a contextMenu property to an element or element class.
     * @param {!Element|!Function} elementOrClass The element or class to add
     *     the contextMenu property to.
     */
    addContextMenuProperty(elementOrClass) {
      const target = typeof elementOrClass === 'function' ?
          elementOrClass.prototype :
          elementOrClass;

      // eslint-disable-next-line no-restricted-properties
      target.__defineGetter__('contextMenu', function() {
        return this.contextMenu_;
      });
      // eslint-disable-next-line no-restricted-properties
      target.__defineSetter__('contextMenu', function(menu) {
        const oldContextMenu = this.contextMenu;

        if (typeof menu === 'string' && menu[0] === '#') {
          menu = this.ownerDocument.getElementById(menu.slice(1));
          decorate(menu, Menu);
        }

        if (menu === oldContextMenu) {
          return;
        }

        if (oldContextMenu && !menu) {
          this.removeEventListener('contextmenu', contextMenuHandler);
          this.removeEventListener('keydown', contextMenuHandler);
          this.removeEventListener('keyup', contextMenuHandler);
        }
        if (menu && !oldContextMenu) {
          this.addEventListener('contextmenu', contextMenuHandler);
          this.addEventListener('keydown', contextMenuHandler);
          this.addEventListener('keyup', contextMenuHandler);
        }

        this.contextMenu_ = menu;

        if (menu && menu.id) {
          this.setAttribute('contextmenu', '#' + menu.id);
        }

        dispatchPropertyChange(this, 'contextMenu', menu, oldContextMenu);
      });

      if (!target.getRectForContextMenu) {
        /**
         * @return {!ClientRect} The rect to use for positioning the context
         *     menu when the context menu is not opened using a mouse position.
         */
        target.getRectForContextMenu = function() {
          return this.getBoundingClientRect();
        };
      }
    }

    /**
     * Sets the given contextMenu to the given element. A contextMenu property
     * would be added if necessary.
     * @param {!Element} element The element or class to set the contextMenu to.
     * @param {!Menu} contextMenu The contextMenu property to be set.
     */
    setContextMenu(element, contextMenu) {
      if (!element.contextMenu) {
        this.addContextMenuProperty(element);
      }
      element.contextMenu = contextMenu;
    }
  }

  /**
   * The singleton context menu handler.
   * @type {!ContextMenuHandler}
   */
  export const contextMenuHandler = new ContextMenuHandler();
