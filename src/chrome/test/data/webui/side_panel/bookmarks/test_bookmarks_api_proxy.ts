// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionSource} from 'chrome://read-later.top-chrome/bookmarks/bookmarks.mojom-webui.js';
import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/bookmarks/bookmarks_api_proxy.js';
import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  private folders_: chrome.bookmarks.BookmarkTreeNode[] = [];
  callbackRouter: {
    onChanged: FakeChromeEvent,
    onChildrenReordered: FakeChromeEvent,
    onCreated: FakeChromeEvent,
    onMoved: FakeChromeEvent,
    onRemoved: FakeChromeEvent,
  };

  constructor() {
    super([
      'getTopLevelBookmarks',
      'getFolders',
      'openBookmark',
      'cutBookmark',
      'copyBookmark',
      'pasteToBookmark',
      'showContextMenu',
      'showUI',
    ]);

    this.callbackRouter = {
      onChanged: new FakeChromeEvent(),
      onChildrenReordered: new FakeChromeEvent(),
      onCreated: new FakeChromeEvent(),
      onMoved: new FakeChromeEvent(),
      onRemoved: new FakeChromeEvent(),
    };
  }

  getTopLevelBookmarks() {
    this.methodCalled('getTopLevelBookmarks');
    return Promise.resolve(this.folders_);
  }

  getFolders() {
    this.methodCalled('getFolders');
    return Promise.resolve(this.folders_);
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.methodCalled('openBookmark', id, depth, clickModifiers, source);
  }

  setFolders(folders: chrome.bookmarks.BookmarkTreeNode[]) {
    this.folders_ = folders;
  }

  copyBookmark(id: string): Promise<void> {
    this.methodCalled('copyBookmark', id);
    return Promise.resolve();
  }

  cutBookmark(id: string) {
    this.methodCalled('cutBookmark', id);
  }

  pasteToBookmark(parentId: string, destinationId?: string): Promise<void> {
    this.methodCalled('pasteToBookmark', parentId, destinationId);
    return Promise.resolve();
  }

  showContextMenu(id: string, x: number, y: number, source: ActionSource) {
    this.methodCalled('showContextMenu', id, x, y, source);
  }

  showUI() {
    this.methodCalled('showUI');
  }
}
