// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enums for BridgeHelper functions.
 */

/**
 * Specifies one of the renderer contexts for the ChromeVox extension. Code
 * specific to each of these contexts is contained in the corresponding
 * directory, while code used by two or more contexts is found in common/.
 * @enum {string}
 */
export const BridgeContext = {
  BACKGROUND: 'background',
  LEARN_MODE: 'learnMode',
  LOG_PAGE: 'logPage',
  OPTIONS: 'options',
  PANEL: 'panel',
};

/**
 * The class that a message is being sent to.
 * @typedef {string}
 */
export let BridgeTarget;

/**
 * @typedef {{ TARGET: string,
 *             Action: !Object}}
 */
let BridgeEntry;

export const BridgeConstants = {};

/** @public {!BridgeEntry} */
BridgeConstants.BrailleBackground = {
  TARGET: 'BrailleBackground',
  Action: {
    BACK_TRANSLATE: 'backTranslate',
    REFRESH_BRAILLE_TABLE: 'refreshBrailleTable',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.BrailleCommandHandler = {
  TARGET: 'BrailleCommandHandler',
  Action: {
    SET_ENABLED: 'setEnabled',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.ChromeVoxPrefs = {
  TARGET: 'ChromeVoxPrefs',
  Action: {
    GET_PREFS: 'getPrefs',
    SET_LOGGING_PREFS: 'setLoggingPrefs',
    SET_PREF: 'setPref',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.ChromeVoxState = {
  TARGET: 'ChromeVoxState',
  Action: {
    CLEAR_CURRENT_RANGE: 'clearCurrentRange',
    UPDATE_PUNCTUATION_ECHO: 'updatePunctuationEcho',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.CommandHandler = {
  TARGET: 'CommandHandler',
  Action: {
    ON_COMMAND: 'onCommand',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.EventSourceState = {
  TARGET: 'EventSourceState',
  Action: {
    GET: 'get',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.EventStreamLogger = {
  TARGET: 'EventStreamLogger',
  Action: {
    NOTIFY_EVENT_STREAM_FILTER_CHANGED: 'notifyEventStreamFilterChanged',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.GestureCommandHandler = {
  TARGET: 'GestureCommandHandler',
  Action: {
    SET_ENABLED: 'setEnabled',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.LogStore = {
  TARGET: 'LogStore',
  Action: {
    CLEAR_LOG: 'clearLog',
    GET_LOGS: 'getLogs',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.Panel = {
  TARGET: 'Panel',
  Action: {
    ADD_MENU_ITEM: 'addMenuItem',
    ON_CURRENT_RANGE_CHANGED: 'onCurrentRangeChanged',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.PanelBackground = {
  TARGET: 'PanelBackground',
  Action: {
    CLEAR_SAVED_NODE: 'clearSavedNode',
    CREATE_ALL_NODE_MENU_BACKGROUNDS: 'createAllNodeMenuBackgrounds',
    CREATE_NEW_I_SEARCH: 'createNewISearch',
    DESTROY_I_SEARCH: 'destroyISearch',
    FOCUS_TAB: 'focusTab',
    GET_ACTIONS_FOR_CURRENT_NODE: 'getActionsForCurrentNode',
    GET_TAB_MENU_DATA: 'getTabMenuData',
    INCREMENTAL_SEARCH: 'incrementalSearch',
    NODE_MENU_CALLBACK: 'nodeMenuCallback',
    PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE: 'performCustomActionOnCurrentNode',
    PERFORM_STANDARD_ACTION_ON_CURRENT_NODE:
        'performStandardActionOnCurrentNode',
    SAVE_CURRENT_NODE: 'saveCurrentNode',
    SET_PANEL_COLLAPSE_WATCHER: 'setPanelCollapseWatcher',
    SET_RANGE_TO_I_SEARCH_NODE: 'setRangeToISearchNode',
    WAIT_FOR_PANEL_COLLAPSE: 'waitForPanelCollapse',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.TtsBackground = {
  TARGET: 'TtsBackground',
  Action: {
    GET_CURRENT_VOICE: 'getCurrentVoice',
  },
};

/** @public {!BridgeEntry} */
BridgeConstants.UserActionMonitor = {
  TARGET: 'UserActionMonitor',
  Action: {
    CREATE: 'create',
    DESTROY: 'destroy',
    ON_KEY_DOWN: 'onKeyDown',
  },
};

/**
 * The action that the message is requesting be performed.
 *
 * This used to be the actions in BridgeConstants, but the module
 * system appears to be confusing the closure compiler and JsDoc.
 * @typedef {string}
 */
export let BridgeAction;
