// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

class SomeClass;

namespace my_namespace {

struct MyStruct {
  // Blocklisted - no rewrite expected.
  SomeClass* my_field;
  SomeClass* my_field2;

  // Non-blocklisted - expected rewrite: raw_ptr<SomeClass> my_field3;
  raw_ptr<SomeClass> my_field3;
};

template <typename T>
class MyTemplate {
 public:
  // Blocklisted - no rewrite expected.
  SomeClass* my_field;

  // Non-blocklisted - expected rewrite: raw_ptr<SomeClass> my_field2;
  raw_ptr<SomeClass> my_field2;
};

}  // namespace my_namespace

namespace other_namespace {

struct MyStruct {
  // Blocklisted in another namespace, but not here.
  // Expected rewrite: raw_ptr<SomeClass> my_field;
  raw_ptr<SomeClass> my_field;
};

}  // namespace other_namespace
