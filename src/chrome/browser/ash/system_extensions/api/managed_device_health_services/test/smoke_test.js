// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
    // This is a "smoke" test that just checks whether the system extension was
    // installed successfully.
    assert_equals(42, 42);
});
