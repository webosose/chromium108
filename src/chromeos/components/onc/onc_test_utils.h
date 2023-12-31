// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {
namespace test_utils {

// Read the file at |filename| as a string. CHECKs if any error occurs.
std::string ReadTestData(const std::string& filename);

// Read a JSON from |filename| and return it as a base::Value.
// CHECKs if any error occurs.
base::Value ReadTestJson(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a base::Value.
// CHECKs if any error occurs.
base::Value ReadTestDictionaryValue(const std::string& filename);

// Checks that the pointer |actual| is not NULL but points to a value that
// equals |expected|. The intended use case is:
// EXPECT_TRUE(test_utils::Equals(expected, actual));
::testing::AssertionResult Equals(const base::Value* expected,
                                  const base::Value* actual);

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::onc::test_utils {
using ::chromeos::onc::test_utils::Equals;
using ::chromeos::onc::test_utils::ReadTestData;
using ::chromeos::onc::test_utils::ReadTestDictionaryValue;
using ::chromeos::onc::test_utils::ReadTestJson;
}  // namespace ash::onc::test_utils

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_TEST_UTILS_H_
