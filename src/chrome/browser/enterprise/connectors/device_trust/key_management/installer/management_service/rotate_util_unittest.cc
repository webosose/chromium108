// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using enterprise_connectors::test::MockKeyNetworkDelegate;
using enterprise_connectors::test::MockKeyPersistenceDelegate;
using enterprise_connectors::test::ScopedKeyPersistenceDelegateFactory;
using HttpResponseCode =
    enterprise_connectors::test::MockKeyNetworkDelegate::HttpResponseCode;

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

constexpr char kNonce[] = "nonce";
constexpr char kEncodedNonce[] = "bm9uY2U=";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kEncodedFakeDMToken[] = "ZmFrZS1icm93c2VyLWRtLXRva2Vu";
constexpr char kFakeDmServerUrl[] =
    "https://m.google.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kInvalidDmServerUrl[] =
    "www.example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kFailureCode = 400;

}  // namespace

namespace enterprise_connectors {

class RotateUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
    auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

    mock_network_delegate_ = mock_network_delegate.get();
    mock_persistence_delegate_ = mock_persistence_delegate.get();

    key_rotation_manager_ = KeyRotationManager::CreateForTesting(
        std::move(mock_network_delegate), std::move(mock_persistence_delegate));
  }

  base::CommandLine GetCommandLine(std::string token,
                                   std::string nonce,
                                   std::string url) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kRotateDTKey, token);
    command_line.AppendSwitchASCII(switches::kNonce, nonce);
    command_line.AppendSwitchASCII(switches::kDmServerUrl, url);
    return command_line;
  }

  MockKeyNetworkDelegate* mock_network_delegate_;
  MockKeyPersistenceDelegate* mock_persistence_delegate_;
  std::unique_ptr<KeyRotationManager> key_rotation_manager_;
  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
  base::test::TaskEnvironment task_environment_;
};

// Tests when the chrome management services key rotation was successful.
TEST_F(RotateUtilTest, RotateDTKeySuccess) {
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));

  EXPECT_TRUE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// an invalid dm token.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidDmToken) {
  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// an incorrectly encoded nonce.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidNonce) {
  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// an invalid dm server url i.e not https or http.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidDMServerUrl) {
  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kInvalidDmServerUrl),
      version_info::Channel::DEV));
}

// Tests when the chrome management services key rotation failed due to
// an invalid rotate command i.e stable channel and non prod host name.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidCommand) {
  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kInvalidDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// incorrect signing key permissions.
TEST_F(RotateUtilTest, RotateDTKeyFailure_PermissionsFailed) {
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(false));

  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// an store key failure.
TEST_F(RotateUtilTest, RotateDTKeyFailure_StoreKeyFailed) {
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

// Tests when the chrome management services key rotation failed due to
// an upload key failure.
TEST_F(RotateUtilTest, RotateDTKeyFailure_UploadKeyFailed) {
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kFailureCode);
      }));

  EXPECT_FALSE(RotateDeviceTrustKey(
      std::move(key_rotation_manager_),
      GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
      version_info::Channel::STABLE));
}

}  // namespace enterprise_connectors
