// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles user keyboard input events.
 */
import {UserActionMonitor} from '../background/user_action_monitor.js';

import {Command} from './command_store.js';
import {KeyMap} from './key_map.js';
import {KeyUtil} from './key_util.js';

export const ChromeVoxKbHandler = {};

/**
 * The key map
 *
 * @type {!KeyMap}
 */
ChromeVoxKbHandler.handlerKeyMap = KeyMap.get();

/**
 * Handler for ChromeVox commands. Returns undefined if the command does not
 * exist. Otherwise, returns the result of executing the command.
 * @type {function(!Command): (boolean|undefined)}
 */
ChromeVoxKbHandler.commandHandler;

/**
 * Converts the key bindings table into an array that is sorted by the lengths
 * of the key bindings. After the sort, the key bindings that describe single
 * keys will come before the key bindings that describe multiple keys.
 * @param {Object<string>} keyToFunctionsTable Contains each key binding and its
 * associated function name.
 * @return {Array<Array<string>>} The sorted key bindings table in
 * array form. Each entry in the array is itself an array containing the
 * key binding and its associated function name.
 * @private
 */
ChromeVoxKbHandler.sortKeyToFunctionsTable_ = function(keyToFunctionsTable) {
  const sortingArray = [];

  for (const keySeqStr in keyToFunctionsTable) {
    sortingArray.push([keySeqStr, keyToFunctionsTable[keySeqStr]]);
  }

  function compareKeyStr(a, b) {
    // Compare the lengths of the key bindings.
    if (a[0].length < b[0].length) {
      return -1;
    } else if (b[0].length < a[0].length) {
      return 1;
    } else {
      // The keys are the same length. Sort lexicographically.
      return a[0].localeCompare(b[0]);
    }
  }

  sortingArray.sort(compareKeyStr);
  return sortingArray;
};


/**
 * Handles key down events.
 *
 * @param {Event} evt The key down event to process.
 * @return {boolean} True if the default action should be performed.
 */
ChromeVoxKbHandler.basicKeyDownActionsListener = function(evt) {
  const keySequence = KeyUtil.keyEventToKeySequence(evt);
  let functionName;
  if (ChromeVoxKbHandler.handlerKeyMap !== undefined) {
    functionName = ChromeVoxKbHandler.handlerKeyMap.commandForKey(keySequence);
  } else {
    functionName = null;
  }

  // TODO (clchen): Disambiguate why functions are null. If the user pressed
  // something that is not a valid combination, make an error noise so there
  // is some feedback.

  if (!functionName) {
    return !KeyUtil.sequencing;
  }

  // This is the key event handler return value - true if the event should
  // propagate and the default action should be performed, false if we eat
  // the key.
  let returnValue = true;
  const commandResult = ChromeVoxKbHandler.commandHandler(functionName);
  if (commandResult !== undefined) {
    returnValue = commandResult;
  } else if (keySequence.cvoxModifier) {
    // Modifier/prefix is active -- prevent default action
    returnValue = false;
  }

  return returnValue;
};
