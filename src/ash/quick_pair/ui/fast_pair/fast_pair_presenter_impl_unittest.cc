// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

namespace {

const std::string kUserEmail = "test@test.test";
const char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr char kValidModelId[] = "718c17";
constexpr char kTestAddress[] = "test_address";
const char kFastPairErrorNotificationId[] =
    "cros_fast_pair_error_notification_id";
const char kFastPairDiscoveryGuestNotificationId[] =
    "cros_fast_pair_discovery_guest_notification_id";
const char kFastPairDiscoveryUserNotificationId[] =
    "cros_fast_pair_discovery_user_notification_id";
const char kFastPairPairingNotificationId[] =
    "cros_fast_pair_pairing_notification_id";
const char kFastPairAssociateAccountNotificationId[] =
    "cros_fast_pair_associate_account_notification_id";
const char kFastPairDiscoverySubsequentNotificationId[] =
    "cros_fast_pair_discovery_subsequent_notification_id";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    notification_.reset();
    for (auto& observer : observer_list())
      observer.OnNotificationRemoved(id, by_user);
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());

    notification_->delegate()->Click(/*button_index=*/button_index,
                                     /*reply=*/absl::nullopt);
  }

  void Close(const std::string& id, bool by_user) {
    if (notification_)
      notification_->delegate()->Close(by_user);
  }

  void RemoveNotificationsForNotifierId(
      const message_center::NotifierId& notifier_id) override {
    remove_notifications_for_notifier_id_ = true;
    notification_.reset();
  }

  bool remove_notifications_for_notifier_id() {
    return remove_notifications_for_notifier_id_;
  }

 private:
  bool remove_notifications_for_notifier_id_;
  std::unique_ptr<message_center::Notification> notification_;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairPresenterImplTest : public AshTestBase {
 public:
  FastPairPresenterImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  FastPairPresenterImplTest(const FastPairPresenterImplTest&) = delete;
  FastPairPresenterImplTest& operator=(const FastPairPresenterImplTest&) =
      delete;

  ~FastPairPresenterImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserEmail, signin::ConsentLevel::kSignin);
    identity_manager_ = identity_test_environment_->identity_manager();

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    repository_ = std::make_unique<FakeFastPairRepository>();

    nearby::fastpair::Device metadata;
    std::string decoded_key;
    base::Base64Decode(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    initially_paired_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairInitial);
    subsequently_paired_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairSubsequent);
    fast_pair_presenter_ =
        std::make_unique<FastPairPresenterImpl>(&test_message_center_);
  }

  void TearDown() override {
    identity_manager_ = nullptr;
    identity_test_environment_.reset();
    fast_pair_presenter_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void OnDiscoveryAction(scoped_refptr<Device> device, DiscoveryAction action) {
    discovery_action_ = action;
  }

  void OnDiscoveryActionForSecondNotification(scoped_refptr<Device> device,
                                              DiscoveryAction action) {
    secondary_discovery_action_ = action;
  }

  void OnPairingFailedAction(scoped_refptr<Device> device,
                             PairingFailedAction action) {
    pairing_failed_action_ = action;
  }

  void OnAssociateAccountAction(scoped_refptr<Device> device,
                                AssociateAccountAction action) {
    associate_account_action_ = action;
  }

 protected:
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  signin::IdentityTestEnvironment identity_test_env_;
  signin::IdentityManager* identity_manager_;
  DiscoveryAction discovery_action_;
  DiscoveryAction secondary_discovery_action_;
  PairingFailedAction pairing_failed_action_;
  AssociateAccountAction associate_account_action_;
  TestMessageCenter test_message_center_;
  scoped_refptr<Device> initially_paired_device_;
  scoped_refptr<Device> subsequently_paired_device_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairPresenter> fast_pair_presenter_;
  base::WeakPtrFactory<FastPairPresenterImplTest> weak_pointer_factory_{this};
};

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedIn_StrictFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_User_OptedOut_StrictFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Child) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_CHILD);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest, RemoveNotifications) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  fast_pair_presenter_->RemoveNotifications(
      /*clear_already_shown_discovery_notification_cache=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.remove_notifications_for_notifier_id());
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_NoDeviceMetadata_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_V1initially_paired_device_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  nearby::fastpair::Device metadata;
  repository_->SetFakeMetadata(kValidModelId, metadata);
  initially_paired_device_ = base::MakeRefCounted<Device>(
      kValidModelId, kTestAddress, Protocol::kFastPairInitial);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_FlagEnabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices,
                            features::kFastPairSavedDevicesStrictOptIn},
      /*disabled_features=*/{});

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(nullptr));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_FlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(nullptr));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest,
       ShowInitialDiscovery_Regular_NoIdentityManager_StrictFlagDisabled) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairSavedDevices},
      /*disabled_features=*/{features::kFastPairSavedDevicesStrictOptIn});

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(nullptr));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_ConnectClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_LearnMoreClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_DismissedByUser) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_User_DismissedByOS) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_KioskApp) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_KIOSK_APP);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kFastPairSavedDevices,
                             features::kFastPairSavedDevicesStrictOptIn});
  repository_->SetOptInStatus(nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_ConnectClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_LearnMoreClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_DismissedByUser) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowInitialDiscovery_Guest_DismissedByOS) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryGuestNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoveryGuestNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowPairing) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairing(initially_paired_device_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairing_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairing(initially_paired_device_);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairPairingNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_SettingsClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairErrorNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(FastPairPresenterImplTest, ShowPairingFailed_DismissedByOS) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowPairingFailed(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnPairingFailedAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairErrorNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairErrorNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pairing_failed_action_, PairingFailedAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_SaveClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_,
            AssociateAccountAction::kAssoicateAccount);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_NoDeviceMetadata) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_NoIdentityManager) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(nullptr));
  repository_->ClearFakeMetadata(kValidModelId);

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_LearnMoreClicked) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairAssociateAccountNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_, AssociateAccountAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_DismissedByUser) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairAssociateAccountNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_,
            AssociateAccountAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowAssociateAccount_DismissedByOS) {
  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowAssociateAccount(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnAssociateAccountAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairAssociateAccountNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairAssociateAccountNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(associate_account_action_, AssociateAccountAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_Connect) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_LearnMore) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(discovery_action_, DiscoveryAction::kLearnMore);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_DismissedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, ShowSubsequentDiscovery_DismissedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissed);
}

TEST_F(FastPairPresenterImplTest, ShowOnlyOneDiscovery_DismissedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();

  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kAlreadyDisplayed);
}

TEST_F(FastPairPresenterImplTest, ShowOnlyOneDiscovery_Connect) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();

  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kAlreadyDisplayed);
}

TEST_F(FastPairPresenterImplTest, ShowDiscoveryAgain_DismissedByOS) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/false);
  base::RunLoop().RunUntilIdle();

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  // If the notification is dismissed by OS, we show it again to the user, who
  // can dismiss it.
  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissed);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kDismissedByUser);
}

TEST_F(FastPairPresenterImplTest, DontShowDiscoveryAgain_DismissedByUser) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.Close(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*by_user=*/true);
  base::RunLoop().RunUntilIdle();

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();

  // If the notification was dismissed by the user, we won't show the
  // notification again.
  EXPECT_EQ(discovery_action_, DiscoveryAction::kDismissedByUser);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kAlreadyDisplayed);
}

TEST_F(FastPairPresenterImplTest, DontShowDiscoveryAgain_ConnectPressed) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();

  // If the user clicks connect on the notification, we won't show it again.
  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kAlreadyDisplayed);
}

TEST_F(FastPairPresenterImplTest, ShowDiscoveryAgain_ProtocolChanged) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  // We expect to be able to interact with this second notification and pair it
  // since it was shown, not dismissed, since it was a different protocol with
  // same address.
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowDiscoveryAgain_MapCleared) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  // Simulate the Bluetooth toggle or Fast Pair toggle being toggled off.
  fast_pair_presenter_->RemoveNotifications(
      /*clear_already_shown_discovery_notification_cache=*/true);

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, ShowDiscoveryAgain_DevicePaired) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));

  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  // Simulate the device being removed from the cache when the device is paired.
  fast_pair_presenter_->RemoveDeviceFromAlreadyShownDiscoveryNotificationCache(
      subsequently_paired_device_);

  fast_pair_presenter_->ShowDiscovery(
      subsequently_paired_device_,
      base::BindRepeating(
          &FastPairPresenterImplTest::OnDiscoveryActionForSecondNotification,
          weak_pointer_factory_.GetWeakPtr(), subsequently_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoverySubsequentNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoverySubsequentNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
  EXPECT_EQ(secondary_discovery_action_, DiscoveryAction::kPairToDevice);
}

TEST_F(FastPairPresenterImplTest, DontShowDiscoveryNotificationsForDeviceLost) {
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  ON_CALL(*browser_delegate_, GetIdentityManager())
      .WillByDefault(testing::Return(identity_manager_));
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  // Simulate 'Device Found' event to show the discovery notification.
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  // Simulate 'Device Lost' event to dismiss all notifications and start the
  // timer to prevent showing notifications for device lost.
  fast_pair_presenter_->RemoveNotifications(
      /*clear_already_shown_discovery_notification_cache=*/false);
  fast_pair_presenter_->StartDeviceLostTimer(initially_paired_device_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.remove_notifications_for_notifier_id());
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));

  // Simulate another 'Device Found' event to replicate poorly behaved devices
  // that are constantly found->lost->found.
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();

  // We expect no notification to be shown for this device since it is within
  // |kFastPairDeviceLostNotificationTimeoutMinutes|.
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  EXPECT_EQ(discovery_action_, DiscoveryAction::kAlreadyDisplayed);

  // After |kFastPairDeviceLostNotificationTimeoutMinutes| is over, we expect to
  // be able to see the notification when we simulate a device found event.
  task_environment()->FastForwardBy(base::Minutes(
      features::kFastPairDeviceLostNotificationTimeoutMinutes.Get()));
  fast_pair_presenter_->ShowDiscovery(
      initially_paired_device_,
      base::BindRepeating(&FastPairPresenterImplTest::OnDiscoveryAction,
                          weak_pointer_factory_.GetWeakPtr(),
                          initially_paired_device_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      kFastPairDiscoveryUserNotificationId));
  test_message_center_.ClickOnNotificationButton(
      /*id=*/kFastPairDiscoveryUserNotificationId, /*button_index=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(discovery_action_, DiscoveryAction::kPairToDevice);
}

}  // namespace quick_pair
}  // namespace ash
