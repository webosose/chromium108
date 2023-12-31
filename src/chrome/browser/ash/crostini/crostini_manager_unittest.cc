// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_manager.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_util.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-shared.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/handlers/powerwash_requirements_checker.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/anomaly_detector/fake_anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crostini {

namespace {

const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kPackageID[] = "package;1;;";
constexpr int64_t kDiskSizeBytes = 4ll * 1024 * 1024 * 1024;  // 4 GiB
const char kTerminaKernelVersion[] =
    "4.19.56-05556-gca219a5b1086 #3 SMP PREEMPT Mon Jul 1 14:36:38 CEST 2019";
const char kCrostiniCorruptionHistogram[] = "Crostini.FilesystemCorruption";
constexpr auto kLongTime = base::Days(10);

void ExpectFailure(base::OnceClosure closure, bool success) {
  EXPECT_FALSE(success);
  std::move(closure).Run();
}

void ExpectSuccess(base::OnceClosure closure, bool success) {
  EXPECT_TRUE(success);
  std::move(closure).Run();
}

void ExpectCrostiniResult(base::OnceClosure closure,
                          CrostiniResult expected_result,
                          CrostiniResult result) {
  EXPECT_EQ(expected_result, result);
  std::move(closure).Run();
}

void ExpectBool(base::OnceClosure closure, bool expected_result, bool result) {
  EXPECT_EQ(expected_result, result);
  std::move(closure).Run();
}

void ExpectCrostiniExportResult(base::OnceClosure closure,
                                CrostiniResult expected_result,
                                uint64_t expected_container_size,
                                uint64_t expected_export_size,
                                CrostiniResult result,
                                uint64_t container_size,
                                uint64_t export_size) {
  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(expected_container_size, container_size);
  EXPECT_EQ(expected_export_size, export_size);
  std::move(closure).Run();
}

class TestRestartObserver : public CrostiniManager::RestartObserver {
 public:
  void OnStageStarted(mojom::InstallerState stage) override {
    stages.push_back(stage);
  }
  std::vector<mojom::InstallerState> stages;
};

}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void SendVmStoppedSignal() {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name(kVmName);
    signal.set_owner_id("test");
    crostini_manager_->OnVmStopped(signal);
  }

  void CreateDiskImageFailureCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      const base::FilePath& file_path) {
    EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      const base::FilePath& file_path) {
    EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
    EXPECT_EQ(result, CrostiniResult::SUCCESS);
    std::move(closure).Run();
  }

  void ListVmDisksSuccessCallback(base::OnceClosure closure,
                                  CrostiniResult result,
                                  int64_t total_size) {
    EXPECT_GE(fake_concierge_client_->list_vm_disks_call_count(), 1);
    std::move(closure).Run();
  }

  base::ScopedFD TestFileDescriptor() {
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    base::ScopedFD fd(file.TakePlatformFile());
    return fd;
  }

  void AttachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_success,
                               bool success,
                               uint8_t guest_port) {
    EXPECT_GE(fake_concierge_client_->attach_usb_device_call_count(), 1);
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void DetachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_called,
                               bool expected_success,
                               bool success) {
    EXPECT_EQ(fake_concierge_client_->detach_usb_device_call_count(),
              expected_called);
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void EnsureTerminaInstalled() {
    base::RunLoop run_loop;
    crostini_manager()->InstallTermina(
        base::BindOnce([](base::OnceClosure callback,
                          CrostiniResult) { std::move(callback).Run(); },
                       run_loop.QuitClosure()),
        /*is_initial_install=*/false);
    run_loop.Run();
  }

  CrostiniManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {
    ash::AnomalyDetectorClient::InitializeFake();
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    fake_cicerone_client_ = ash::FakeCiceroneClient::Get();
    fake_concierge_client_ = ash::FakeConciergeClient::Get();
    fake_anomaly_detector_client_ =
        static_cast<ash::FakeAnomalyDetectorClient*>(
            ash::AnomalyDetectorClient::Get());
  }

  CrostiniManagerTest(const CrostiniManagerTest&) = delete;
  CrostiniManagerTest& operator=(const CrostiniManagerTest&) = delete;

  ~CrostiniManagerTest() override {
    ash::AnomalyDetectorClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void SetUp() override {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeCrosComponentManager(component_manager_);
    ash::DlcserviceClient::InitializeFake();

    scoped_feature_list_.InitWithFeatures({features::kCrostini}, {});
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());

    // Login user for crostini, link gaia for DriveFS.
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    AccountId account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    mojo::Remote<device::mojom::UsbDeviceManager> fake_usb_manager;
    fake_usb_manager_.AddReceiver(
        fake_usb_manager.BindNewPipeAndPassReceiver());

    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();

    guest_os::GuestOsService::GetForProfile(profile())
        ->WaylandServer()
        ->OverrideServerForTesting(vm_tools::launch::TERMINA, nullptr, {});

    ash::CryptohomeMiscClient::InitializeFake();
    ash::FakeCryptohomeMiscClient::Get()->set_requires_powerwash(false);
    policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());

    vm_tools::cicerone::OsRelease os_release;
    base::HistogramTester histogram_tester{};
    os_release.set_pretty_name("Debian GNU/Linux 10 (bullseye)");
    os_release.set_version_id("11");
    os_release.set_id("debian");
    fake_cicerone_client_->set_lxd_container_os_release(os_release);
  }

  void TearDown() override {
    ash::CryptohomeMiscClient::Shutdown();
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    scoped_user_manager_.reset();
    crostini_manager_->Shutdown();
    profile_.reset();
    run_loop_.reset();
    ash::DlcserviceClient::Shutdown();
    browser_part_.ShutdownCrosComponentManager();
    component_manager_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniManager* crostini_manager() { return crostini_manager_; }
  const guest_os::GuestId& container_id() { return container_id_; }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  ash::FakeCiceroneClient* fake_cicerone_client_;
  ash::FakeConciergeClient* fake_concierge_client_;
  ash::FakeAnomalyDetectorClient* fake_anomaly_detector_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  CrostiniManager* crostini_manager_;
  const guest_os::GuestId container_id_ =
      guest_os::GuestId(kCrostiniDefaultVmType, kVmName, kContainerName);
  device::FakeUsbDeviceManager fake_usb_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
};

TEST_F(CrostiniManagerTest, CreateDiskImageEmptyNameError) {
  crostini_manager()->CreateDiskImage(
      "", vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageFailureCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  crostini_manager()->CreateDiskImage(
      kVmName,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageFailureCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  crostini_manager()->CreateDiskImage(
      kVmName, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageEmptyNameError) {
  crostini_manager()->DestroyDiskImage(
      "", base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->destroy_disk_image_call_count(), 0);
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  crostini_manager()->DestroyDiskImage(
      kVmName, base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->destroy_disk_image_call_count(), 1);
}

TEST_F(CrostiniManagerTest, ListVmDisksSuccess) {
  crostini_manager()->ListVmDisks(
      base::BindOnce(&CrostiniManagerTest::ListVmDisksSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  const base::FilePath& disk_path = base::FilePath("unused");

  crostini_manager()->StartTerminaVm(
      "", disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmAnomalyDetectorNotConnectedError) {
  const base::FilePath& disk_path = base::FilePath("unused");

  fake_anomaly_detector_client_->set_guest_file_corruption_signal_connected(
      false);

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmPowerwashRequestError) {
  const base::FilePath& disk_path = base::FilePath("unused");

  // Login unaffiliated user.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), "0987654321"));
  fake_user_manager()->AddUserWithAffiliation(account_id, false);
  fake_user_manager()->LoginUser(account_id);

  // Set DeviceRebootOnUserSignout to always.
  ash::ScopedCrosSettingsTestHelper settings_helper{
      /* create_settings_service=*/false};
  settings_helper.ReplaceDeviceSettingsProviderWithStub();
  settings_helper.SetInteger(
      ash::kDeviceRebootOnUserSignout,
      enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);

  // Set cryptohome requiring powerwash.
  ash::FakeCryptohomeMiscClient::Get()->set_requires_powerwash(true);
  policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();

  NotificationDisplayServiceTester notification_service(profile());

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);

  auto notification = notification_service.GetNotification(
      "crostini_powerwash_request_instead_of_run");
  EXPECT_NE(absl::nullopt, notification);
}

TEST_F(CrostiniManagerTest,
       StartTerminaVmPowerwashRequestErrorDueToCryptohomeError) {
  const base::FilePath& disk_path = base::FilePath("unused");

  // Login unaffiliated user.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), "0987654321"));
  fake_user_manager()->AddUserWithAffiliation(account_id, false);
  fake_user_manager()->LoginUser(account_id);

  // Set DeviceRebootOnUserSignout to always.
  ash::ScopedCrosSettingsTestHelper settings_helper{
      /* create_settings_service=*/false};
  settings_helper.ReplaceDeviceSettingsProviderWithStub();
  settings_helper.SetInteger(
      ash::kDeviceRebootOnUserSignout,
      enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);

  // Reset cryptohome state to undefined and make cryptohome unavailable.
  policy::PowerwashRequirementsChecker::ResetForTesting();
  ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);
  policy::PowerwashRequirementsChecker::Initialize();
  ash::FakeCryptohomeMiscClient::Get()->ReportServiceIsNotAvailable();

  NotificationDisplayServiceTester notification_service(profile());

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);

  auto notification = notification_service.GetNotification(
      "crostini_powerwash_request_cryptohome_error");
  EXPECT_NE(absl::nullopt, notification);
}

TEST_F(CrostiniManagerTest, StartTerminaVmMountError) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  response.set_mount_result(vm_tools::concierge::StartVmResponse::FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::MOUNT_FAILED, 1);
}

TEST_F(CrostiniManagerTest, StartTerminaVmMountErrorThenSuccess) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_STARTING);
  response.set_mount_result(
      vm_tools::concierge::StartVmResponse::PARTIAL_DATA_LOSS);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::MOUNT_ROLLED_BACK, 1);
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectTotalCount(kCrostiniCorruptionHistogram, 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmLowDiskNotification) {
  const base::FilePath& disk_path = base::FilePath("unused");
  NotificationDisplayServiceTester notification_service(nullptr);
  vm_tools::concierge::StartVmResponse response;
  response.set_free_bytes(0);
  response.set_free_bytes_has_value(true);
  response.set_success(true);
  response.set_status(::vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      DefaultContainerId().vm_name, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  auto notification = notification_service.GetNotification("crostini_low_disk");
  EXPECT_NE(absl::nullopt, notification);
}

TEST_F(CrostiniManagerTest,
       StartTerminaVmLowDiskNotificationNotShownIfNoValue) {
  const base::FilePath& disk_path = base::FilePath("unused");
  NotificationDisplayServiceTester notification_service(nullptr);
  vm_tools::concierge::StartVmResponse response;
  response.set_free_bytes(1234);
  response.set_free_bytes_has_value(false);
  response.set_success(true);
  response.set_status(::vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      DefaultContainerId().vm_name, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  auto notification = notification_service.GetNotification("crostini_low_disk");
  EXPECT_EQ(absl::nullopt, notification);
}

TEST_F(CrostiniManagerTest, OnStartTremplinRecordsRunningVm) {
  const base::FilePath& disk_path = base::FilePath("unused");
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));

  // Check that the Vm start is not recorded until tremplin starts.
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));
  run_loop()->Run();
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
}

TEST_F(CrostiniManagerTest, OnStartTremplinHappensEarlier) {
  fake_concierge_client_->set_send_tremplin_started_signal_delay(
      base::Milliseconds(1));
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::Milliseconds(10));
  const base::FilePath& disk_path = base::FilePath("unused");
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  EnsureTerminaInstalled();
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));

  // Check that the Vm start is not recorded until tremplin starts.
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));
  run_loop()->Run();
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  crostini_manager()->StopVm(
      "", base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                         CrostiniResult::CLIENT_ERROR));
  run_loop()->Run();
  EXPECT_EQ(fake_concierge_client_->stop_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  crostini_manager()->StopVm(
      kVmName, base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                              CrostiniResult::SUCCESS));
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->stop_vm_call_count(), 1);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageRootAccessError) {
  FakeCrostiniFeatures crostini_features;
  crostini_features.set_root_access_allowed(false);
  crostini_manager()->InstallLinuxPackage(
      container_id(), "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackage(
      container_id(), "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      container_id(), "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  std::string failure_reason = "Unit tests can't install Linux packages!";
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(failure_reason);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      container_id(), "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      container_id(), "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalNotConnectedError) {
  fake_cicerone_client_->set_uninstall_package_progress_signal_connected(false);
  crostini_manager()->UninstallPackageOwningFile(
      container_id(), "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalSuccess) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::STARTED);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      container_id(), "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalFailure) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED);
  response.set_failure_reason("Didn't feel like it");
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      container_id(), "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalOperationBlocked) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(vm_tools::cicerone::UninstallPackageOwningFileResponse::
                          BLOCKING_OPERATION_IN_PROGRESS);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      container_id(), "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, RegisterContainerPrefWhenContainerCreated) {
  const base::Value::List* pref =
      &profile_->GetPrefs()->GetList(guest_os::prefs::kGuestOsContainers);
  EXPECT_EQ(pref->size(), 0u);
  crostini_manager()->CreateLxdContainer(
      container_id(), absl::nullopt, absl::nullopt,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
  pref = &profile_->GetPrefs()->GetList(guest_os::prefs::kGuestOsContainers);
  EXPECT_EQ(pref->size(), 1u);
}

TEST_F(CrostiniManagerTest, RegisterContainerPrefWhenContainerExists) {
  const base::Value::List* pref =
      &profile_->GetPrefs()->GetList(guest_os::prefs::kGuestOsContainers);
  EXPECT_EQ(pref->size(), 0u);
  vm_tools::cicerone::CreateLxdContainerResponse response;
  response.set_status(vm_tools::cicerone::CreateLxdContainerResponse::EXISTS);
  fake_cicerone_client_->set_create_lxd_container_response(response);
  crostini_manager()->CreateLxdContainer(
      container_id(), absl::nullopt, absl::nullopt,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
  pref = &profile_->GetPrefs()->GetList(guest_os::prefs::kGuestOsContainers);
  EXPECT_EQ(pref->size(), 1u);
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();

    // Most tests are for a non-initial install, so return by default that a
    // disk already exists to avoid extra error logs.
    SetCreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_EXISTS);
  }

  void RestartCrostiniCallback(base::OnceClosure closure,
                               CrostiniResult result) {
    restart_crostini_callback_count_++;
    last_crostini_callback_result_ = result;
    std::move(closure).Run();
  }

  void RemoveCrostiniCallback(base::OnceClosure closure,
                              CrostiniResult result) {
    remove_crostini_callback_count_++;
    std::move(closure).Run();
  }

  // CrostiniManager::RestartObserver
  void OnStageStarted(mojom::InstallerState stage) override {
    on_stage_started_.Run(stage);
  }

  void OnComponentLoaded(CrostiniResult result) override {
    if (cancel_on_component_loaded_) {
      Cancel();
    }
  }

  void OnDiskImageCreated(bool success,
                          CrostiniResult result,
                          int64_t disk_size_available) override {
    if (cancel_on_disk_image_created_) {
      Cancel();
    }
  }

  void OnVmStarted(bool success) override {
    if (cancel_on_vm_started_) {
      Cancel();
    }
  }

  void OnLxdStarted(CrostiniResult result) override {
    if (cancel_on_lxd_started_) {
      Cancel();
    }
  }

  void OnContainerCreated(CrostiniResult result) override {
    if (cancel_on_container_created_) {
      Cancel();
    }
    if (cancel_then_stop_vm_) {
      // Don't use the Cancel() method because it terminates the run loop
      // immediately, and we want to wait for the OnVmStopped task to complete.
      crostini_manager()->CancelRestartCrostini(restart_id_);

      // Signal that the VM has stopped by posting a task to avoid deleting
      // CrostiniRestarter inside a CrostiniRestarter call.
      vm_tools::concierge::VmStoppedSignal signal;
      signal.set_owner_id(CryptohomeIdForProfile(profile()));
      signal.set_name(kVmName);
      base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&CrostiniManager::OnVmStopped,
                         base::Unretained(crostini_manager()), signal),
          run_loop()->QuitClosure());
    }
  }

  void OnContainerStarted(CrostiniResult result) override {
    if (cancel_on_container_started_) {
      Cancel();
    }
  }

  void OnContainerSetup(bool success) override {
    if (cancel_on_container_setup_) {
      Cancel();
    }
  }

 protected:
  void ExpectCrostiniRestartResult(CrostiniResult result) {
    EXPECT_EQ(1, restart_crostini_callback_count_);
    EXPECT_EQ(result, last_crostini_callback_result_);
  }

  void Cancel() {
    crostini_manager()->CancelRestartCrostini(restart_id_);
    run_loop()->Quit();
  }

  void ExpectRestarterUmaCount(int count) {
    histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", count);
    histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", count);
    histogram_tester_.ExpectTotalCount("Crostini.Installer.Started", 0);
  }

  void SetCreateDiskImageResponse(vm_tools::concierge::DiskImageStatus status) {
    vm_tools::concierge::CreateDiskImageResponse response;
    response.set_status(status);
    response.set_disk_path("foo");
    fake_concierge_client_->set_create_disk_image_response(response);
  }

  CrostiniManager::RestartId restart_id_ =
      CrostiniManager::kUninitializedRestartId;
  const CrostiniManager::RestartId uninitialized_id_ =
      CrostiniManager::kUninitializedRestartId;
  bool cancel_on_component_loaded_ = false;
  bool cancel_on_disk_image_created_ = false;
  bool cancel_on_vm_started_ = false;
  bool cancel_on_lxd_started_ = false;
  bool cancel_on_container_created_ = false;
  bool cancel_on_container_started_ = false;
  bool cancel_on_container_setup_ = false;
  bool cancel_then_stop_vm_ = false;

  int restart_crostini_callback_count_ = 0;
  CrostiniResult last_crostini_callback_result_ = CrostiniResult::SUCCESS;
  int remove_crostini_callback_count_ = 0;
  ash::disks::MockDiskMountManager* disk_mount_manager_mock_;
  base::HistogramTester histogram_tester_{};

  base::RepeatingCallback<void(mojom::InstallerState)> on_stage_started_ =
      base::DoNothing();
};

TEST_F(CrostiniManagerRestartTest, RestartSuccess) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(1, restart_crostini_callback_count_);
  auto req = fake_cicerone_client_->get_setup_lxd_container_user_request();
  EXPECT_EQ(req.container_username(),
            DefaultContainerUserNameForProfile(profile()));

  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterTimeInState2.Start", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.StartContainer", 1);
  histogram_tester_.ExpectBucketCount(
      "Crostini.SetUpLxdContainerUser.UnknownResult", false, 1);
}

TEST_F(CrostiniManagerRestartTest, CrostiniNotAllowed) {
  FakeCrostiniFeatures crostini_features;
  crostini_features.set_is_allowed_now(false);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  ExpectCrostiniRestartResult(CrostiniResult::NOT_ALLOWED);
  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));
  histogram_tester_.ExpectBucketCount("Crostini.RestarterResult",
                                      CrostiniResult::NOT_ALLOWED, 1);
}

TEST_F(CrostiniManagerRestartTest, UncleanRestartReportsMetricToUncleanBucket) {
  crostini_manager()->SetUncleanStartupForTesting(true);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(1, restart_crostini_callback_count_);

  histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 1);
  histogram_tester_.ExpectTotalCount("Crostini.Installer.Started", 0);
}

TEST_F(CrostiniManagerRestartTest, RestartDelayAndSuccessWhenVmStopping) {
  crostini_manager()->AddStoppingVmForTesting(kVmName);
  on_stage_started_ =
      base::BindLambdaForTesting([this](mojom::InstallerState state) {
        if (state == mojom::InstallerState::kStart) {
          // This tells the restarter that the vm has stopped, and it should
          // resume the restarting process.
          SendVmStoppedSignal();
        }
      });
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(1, restart_crostini_callback_count_);

  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, RestartSuccessWithOptions) {
  CrostiniManager::RestartOptions options;
  options.container_username = "helloworld";
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(1, restart_crostini_callback_count_);
  auto req = fake_cicerone_client_->get_setup_lxd_container_user_request();
  EXPECT_EQ(req.container_username(), "helloworld");
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnComponentLoaded) {
  cancel_on_component_loaded_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled));
  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringComponentLoaded) {
  crostini_manager()->SetInstallTerminaNeverCompletesForTesting(true);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  ExpectCrostiniRestartResult(CrostiniResult::INSTALL_IMAGE_LOADER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.InstallImageLoader", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.CreateDiskImage", 0);
}

TEST_F(CrostiniManagerRestartTest, CancelOnDiskImageCreated) {
  cancel_on_disk_image_created_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.InstallImageLoader", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.CreateDiskImage", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.StartTerminaVm", 0);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringCreateDiskImage) {
  fake_concierge_client_->set_send_create_disk_image_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
  ExpectCrostiniRestartResult(CrostiniResult::CREATE_DISK_IMAGE_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, UnexpectedTransitionsRecorded) {
  fake_concierge_client_->set_send_create_disk_image_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  // Run until we're sitting in the CreateDiskImage step that'll never return,
  // then try triggering a different transition.
  run_loop()->RunUntilIdle();
  crostini_manager()->CallRestarterStartLxdContainerFinishedForTesting(
      restart_id_, CrostiniResult::CONTAINER_START_FAILED);
  histogram_tester_.ExpectUniqueSample("Crostini.InvalidStateTransition",
                                       mojom::InstallerState::kStartContainer,
                                       1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnVmStarted) {
  cancel_on_vm_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartVm) {
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_TERMINA_VM_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForVmStarted) {
  fake_concierge_client_->set_send_tremplin_started_signal_delay(
      base::TimeDelta::Max());
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_STARTING);
  fake_concierge_client_->set_start_vm_response(response);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_TERMINA_VM_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnLxdStarted) {
  cancel_on_lxd_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartLxd) {
  fake_cicerone_client_->set_send_start_lxd_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_LXD_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForLxdStarted) {
  vm_tools::cicerone::StartLxdResponse response;
  response.set_status(vm_tools::cicerone::StartLxdResponse::STARTING);
  fake_cicerone_client_->set_start_lxd_response(response);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_LXD_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnContainerCreated) {
  cancel_on_container_created_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringCreateContainer) {
  fake_cicerone_client_->set_send_create_lxd_container_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  ExpectCrostiniRestartResult(CrostiniResult::CREATE_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForContainerCreated) {
  fake_cicerone_client_->set_send_notify_lxd_container_created_signal_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::CREATE_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, HeartbeatKeepsCreateContainerFromTimingOut) {
  fake_cicerone_client_->set_send_notify_lxd_container_created_signal_delay(
      base::TimeDelta::Max());
  vm_tools::cicerone::LxdContainerDownloadingSignal signal;
  signal.set_container_name(DefaultContainerId().container_name);
  signal.set_vm_name(DefaultContainerId().vm_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));

  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);

  task_environment_.FastForwardBy(base::Minutes(4));
  crostini_manager_->OnLxdContainerDownloading(signal);
  task_environment_.FastForwardBy(base::Minutes(4));
  ASSERT_EQ(0, restart_crostini_callback_count_);

  task_environment_.FastForwardBy(base::Minutes(6));
  ASSERT_EQ(1, restart_crostini_callback_count_);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::CREATE_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnContainerCreatedError) {
  cancel_on_container_started_ = true;
  fake_cicerone_client_->set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN);
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNKNOWN_ERROR),
      this);
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(0, restart_crostini_callback_count_);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnContainerStarted) {
  cancel_on_container_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelOnContainerSetup) {
  cancel_on_container_setup_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringContainerSetup) {
  fake_cicerone_client_->set_send_set_up_lxd_container_user_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::SETUP_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartContainer) {
  fake_cicerone_client_->set_send_start_lxd_container_response_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  ExpectCrostiniRestartResult(CrostiniResult::START_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForContainerStarted) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest,
       HeartbeatKeepsContainerStartedFromTimingOut) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());
  vm_tools::cicerone::LxdContainerStartingSignal signal;
  signal.set_container_name(DefaultContainerId().container_name);
  signal.set_vm_name(DefaultContainerId().vm_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_status(vm_tools::cicerone::LxdContainerStartingSignal::STARTING);

  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);

  task_environment_.FastForwardBy(base::Minutes(7));
  crostini_manager_->OnLxdContainerStarting(signal);
  task_environment_.FastForwardBy(base::Minutes(7));
  ASSERT_EQ(0, restart_crostini_callback_count_);

  task_environment_.FastForwardBy(base::Minutes(2));
  ASSERT_EQ(1, restart_crostini_callback_count_);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::START_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelThenStopVm) {
  cancel_then_stop_vm_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      this);
  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelFinishedRestartIsSafe) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  ExpectCrostiniRestartResult(CrostiniResult::SUCCESS);

  base::RunLoop run_loop;
  crostini_manager()->CancelRestartCrostini(restart_id_);
  run_loop.RunUntilIdle();
  // Just make sure nothing crashes.
}

TEST_F(CrostiniManagerRestartTest, DoubleCancelIsSafe) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);

  crostini_manager()->CancelRestartCrostini(restart_id_);
  crostini_manager()->CancelRestartCrostini(restart_id_);
  run_loop()->Run();
  ExpectCrostiniRestartResult(CrostiniResult::RESTART_REQUEST_CANCELLED);
}

TEST_F(CrostiniManagerRestartTest, MultiRestartAllowed) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id2 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id3 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_EQ(3, restart_crostini_callback_count_);

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
  ExpectRestarterUmaCount(3);
}

TEST_F(CrostiniManagerRestartTest, FailureWithMultipleRestarts) {
  // When multiple restarters are running, a failure in the first should cause
  // the others to fail immediately.

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  auto barrier_closure = base::BarrierClosure(3, run_loop()->QuitClosure());
  auto result_callback =
      base::BindLambdaForTesting([barrier_closure](CrostiniResult result) {
        EXPECT_EQ(CrostiniResult::VM_START_FAILED, result);
        barrier_closure.Run();
      });
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(container_id(), result_callback);
  id2 = crostini_manager()->RestartCrostini(container_id(), result_callback);
  id3 = crostini_manager()->RestartCrostini(container_id(), result_callback);

  run_loop()->Run();

  EXPECT_EQ(1, fake_concierge_client_->start_vm_call_count());
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
}

TEST_F(CrostiniManagerRestartTest, InstallHistogramEntries) {
  // When the first request is tagged as RestartSource::kInstaller, we should
  // log a single result to Crostini.RestarterResult.Installer and no results
  // to Crostini.RestartResult even if there were additional requests.

  SetCreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED);

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  auto barrier_closure = base::BarrierClosure(2, run_loop()->QuitClosure());
  auto result_callback =
      base::BindLambdaForTesting([&barrier_closure](CrostiniResult result) {
        EXPECT_EQ(CrostiniResult::VM_START_FAILED, result);
        barrier_closure.Run();
      });
  CrostiniManager::RestartOptions options1;
  options1.restart_source = RestartSource::kInstaller;
  crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options1), result_callback);
  crostini_manager()->RestartCrostini(container_id(), result_callback);
  run_loop()->Run();

  histogram_tester_.ExpectBucketCount("Crostini.RestarterResult.Installer",
                                      CrostiniResult::VM_START_FAILED, 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 0);

  // Likewise for RestartSource::kMultiContainerCreation
  base::RunLoop run_loop2;
  barrier_closure = base::BarrierClosure(2, run_loop2.QuitClosure());
  CrostiniManager::RestartOptions options2;
  options2.restart_source = RestartSource::kMultiContainerCreation;
  crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options2), result_callback);
  crostini_manager()->RestartCrostini(container_id(), result_callback);
  run_loop2.Run();

  histogram_tester_.ExpectBucketCount(
      "Crostini.RestarterResult.MultiContainerCreation",
      CrostiniResult::VM_START_FAILED, 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult.Installer", 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 0);
}

TEST_F(CrostiniManagerRestartTest, IsContainerRunningFalseIfVmNotStarted) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  // Mount only performed for termina/penguin.
  EXPECT_EQ(1, restart_crostini_callback_count_);

  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));

  // Now call StartTerminaVm again. The default response state is "STARTING",
  // so no container should be considered running.
  const base::FilePath& disk_path = base::FilePath("unused");

  base::RunLoop run_loop2;
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path, {}, 0,
      base::BindOnce(&ExpectSuccess, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, OsReleaseSetCorrectly) {
  vm_tools::cicerone::OsRelease os_release;
  base::HistogramTester histogram_tester{};
  os_release.set_pretty_name("Debian GNU/Linux 10 (buster)");
  os_release.set_version_id("10");
  os_release.set_id("debian");
  fake_cicerone_client_->set_lxd_container_os_release(os_release);

  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  run_loop()->Run();

  const auto* stored_os_release =
      crostini_manager()->GetContainerOsRelease(container_id());
  EXPECT_NE(stored_os_release, nullptr);
  // Sadly, we can't use MessageDifferencer here because we're using the LITE
  // API in our protos.
  EXPECT_EQ(os_release.SerializeAsString(),
            stored_os_release->SerializeAsString());
  histogram_tester.ExpectUniqueSample("Crostini.ContainerOsVersion",
                                      ContainerOsVersion::kDebianBuster, 1);

  // The data for this container should also be stored in prefs.
  const base::Value* os_release_pref_value = GetContainerPrefValue(
      profile(), container_id(), guest_os::prefs::kContainerOsVersionKey);
  EXPECT_NE(os_release_pref_value, nullptr);
  EXPECT_EQ(os_release_pref_value->GetInt(),
            static_cast<int>(ContainerOsVersion::kDebianBuster));
}

TEST_F(CrostiniManagerRestartTest, RestartThenUninstall) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  ExpectCrostiniRestartResult(CrostiniResult::RESTART_ABORTED);
  EXPECT_EQ(1, remove_crostini_callback_count_);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, RestartMultipleThenUninstall) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));
  id2 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));
  id3 = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();

  EXPECT_EQ(3, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
  ExpectRestarterUmaCount(3);
}

TEST_F(CrostiniManagerRestartTest, UninstallWithRestarterTimeout) {
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::TimeDelta::Max());
  on_stage_started_ =
      base::BindLambdaForTesting([&](mojom::InstallerState state) {
        if (state == mojom::InstallerState::kStartTerminaVm)
          run_loop()->Quit();
      });
  restart_id_ = crostini_manager()->RestartCrostini(container_id(),
                                                    base::DoNothing(), this);
  run_loop()->Run();

  // In the kStartTerminaVm state now. Start an uninstall and then wait for
  // the timeout to be hit.

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));

  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, UninstallThenRestart) {
  // Install crostini first so that the uninstaller doesn't terminate before we
  // can call the installer again
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  base::RunLoop run_loop2;
  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop2.QuitClosure()));

  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()));

  EXPECT_EQ(uninitialized_id_, restart_id_);

  run_loop2.Run();

  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, VmStoppedDuringRestart) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  EXPECT_EQ(0, restart_crostini_callback_count_);
  vm_tools::concierge::VmStoppedSignal vm_stopped_signal;
  vm_stopped_signal.set_owner_id(CryptohomeIdForProfile(profile()));
  vm_stopped_signal.set_name(kVmName);
  crostini_manager()->OnVmStopped(vm_stopped_signal);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));
  EXPECT_EQ(1, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, RestartTriggersArcSideloadIfEnabled) {
  ash::SessionManagerClient::InitializeFake();
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(true);

  vm_tools::cicerone::ConfigureForArcSideloadResponse fake_response;
  fake_response.set_status(
      vm_tools::cicerone::ConfigureForArcSideloadResponse::SUCCEEDED);
  fake_cicerone_client_->set_enable_arc_sideload_response(fake_response);

  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_cicerone_client_->configure_for_arc_sideload_called());
}

TEST_F(CrostiniManagerRestartTest, RestartDoesNotTriggerArcSideloadIfDisabled) {
  ash::SessionManagerClient::InitializeFake();
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(false);

  vm_tools::cicerone::ConfigureForArcSideloadResponse fake_response;
  fake_response.set_status(
      vm_tools::cicerone::ConfigureForArcSideloadResponse::SUCCEEDED);
  fake_cicerone_client_->set_enable_arc_sideload_response(fake_response);

  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_cicerone_client_->configure_for_arc_sideload_called());
}

TEST_F(CrostiniManagerRestartTest, RestartWhileShuttingDown) {
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  // crostini_manager() is destructed during test teardown, mimicking the effect
  // of shutting down chrome while a restart is running.
}

TEST_F(CrostiniManagerRestartTest, AllObservers) {
  TestRestartObserver observer2;
  int observer1_count = 0;
  on_stage_started_ =
      base::BindLambdaForTesting([&](mojom::InstallerState state) {
        ++observer1_count;
        if (state == mojom::InstallerState::kStartTerminaVm) {
          // Add a second Restarter with observer while first is starting.
          crostini_manager()->RestartCrostini(
              container_id(),
              base::BindOnce(
                  &CrostiniManagerRestartTest::RestartCrostiniCallback,
                  base::Unretained(this), run_loop()->QuitClosure()),
              &observer2);
        }
      });

  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      this);
  run_loop()->Run();
  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(8, observer1_count);
  EXPECT_EQ(5u, observer2.stages.size());
}

TEST_F(CrostiniManagerRestartTest, StartVmOnly) {
  TestRestartObserver observer;
  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      &observer);
  run_loop()->Run();
  ExpectCrostiniRestartResult(CrostiniResult::SUCCESS);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer.stages);
}

TEST_F(CrostiniManagerRestartTest, StartVmOnlyThenFullRestart) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      &observer1);
  crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      &observer2);
  run_loop()->Run();
  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, FullRestartThenStartVmOnly) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      &observer1);
  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      &observer2);
  run_loop()->Run();
  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, StartVmOnlyTwice) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  CrostiniManager::RestartOptions options1;
  options1.start_vm_only = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options1),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      &observer1);
  CrostiniManager::RestartOptions options2;
  options2.start_vm_only = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options2),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      &observer2);
  run_loop()->Run();
  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, StopAfterLxdAvailableThenFullRestart) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  CrostiniManager::RestartOptions options;
  options.stop_after_lxd_available = true;
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      container_id(), std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing()),
      &observer1);
  crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      &observer2);
  run_loop()->Run();
  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, UninstallUnregistersContainers) {
  auto* registry = guest_os::GuestOsService::GetForProfile(profile_.get())
                       ->TerminalProviderRegistry();
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindLambdaForTesting([this, registry](CrostiniResult result) {
        ASSERT_GT(registry->List().size(), 0u);
        crostini_manager()->RemoveCrostini(
            kVmName,
            base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                           base::Unretained(this), run_loop()->QuitClosure()));
      }));
  run_loop()->Run();
  ASSERT_EQ(registry->List().size(), 0u);
}

TEST_F(CrostiniManagerRestartTest,
       DeleteUnregistersContainersWhenDoesNotExist) {
  auto* registry = guest_os::GuestOsService::GetForProfile(profile_.get())
                       ->TerminalProviderRegistry();
  vm_tools::cicerone::DeleteLxdContainerResponse response;
  response.set_status(
      vm_tools::cicerone::DeleteLxdContainerResponse::DOES_NOT_EXIST);
  fake_cicerone_client_->set_delete_lxd_container_response_(response);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindLambdaForTesting([this, registry](CrostiniResult result) {
        ASSERT_GT(registry->List().size(), 0u);
        crostini_manager()->DeleteLxdContainer(
            container_id(),
            base::BindOnce(&ExpectBool, run_loop()->QuitClosure(), true));
      }));
  run_loop()->Run();
  ASSERT_EQ(registry->List().size(), 0u);
}

TEST_F(CrostiniManagerRestartTest, DeleteUnregistersContainers) {
  auto* registry = guest_os::GuestOsService::GetForProfile(profile_.get())
                       ->TerminalProviderRegistry();
  vm_tools::cicerone::LxdContainerDeletedSignal signal;
  signal.set_vm_name(container_id().vm_name);
  signal.set_container_name(container_id().container_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_status(vm_tools::cicerone::LxdContainerDeletedSignal::DELETED);
  restart_id_ = crostini_manager()->RestartCrostini(
      container_id(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
  ASSERT_GT(registry->List().size(), 0u);
  crostini_manager()->OnLxdContainerDeleted(signal);
  ASSERT_EQ(registry->List().size(), 0u);
}

class CrostiniManagerEnterpriseReportingTest
    : public CrostiniManagerRestartTest {
 public:
  void SetUp() override {
    CrostiniManagerRestartTest::SetUp();

    // Enable Crostini reporting:
    profile()->GetPrefs()->SetBoolean(prefs::kReportCrostiniUsageEnabled, true);
  }

  void TearDown() override {
    ash::disks::DiskMountManager::Shutdown();
    CrostiniManagerRestartTest::TearDown();
  }
};

TEST_F(CrostiniManagerEnterpriseReportingTest,
       LogKernelVersionForEnterpriseReportingSuccess) {
  // Set success response for retrieving enterprise reporting info:
  vm_tools::concierge::GetVmEnterpriseReportingInfoResponse response;
  response.set_success(true);
  response.set_vm_kernel_version(kTerminaKernelVersion);
  fake_concierge_client_->set_get_vm_enterprise_reporting_info_response(
      response);

  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_call_count());
  EXPECT_EQ(1, restart_crostini_callback_count_);
  EXPECT_EQ(kTerminaKernelVersion,
            profile()->GetPrefs()->GetString(
                crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion));
}

TEST_F(CrostiniManagerEnterpriseReportingTest,
       LogKernelVersionForEnterpriseReportingFailure) {
  // Set error response for retrieving enterprise reporting info:
  vm_tools::concierge::GetVmEnterpriseReportingInfoResponse response;
  response.set_success(false);
  response.set_failure_reason("Don't feel like it");
  fake_concierge_client_->set_get_vm_enterprise_reporting_info_response(
      response);

  restart_id_ = crostini_manager()->RestartCrostini(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_call_count());
  EXPECT_EQ(1, restart_crostini_callback_count_);
  // In case of an error, the pref should be (re)set to the empty string:
  EXPECT_TRUE(
      profile()
          ->GetPrefs()
          ->GetString(crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion)
          .empty());
}

TEST_F(CrostiniManagerTest, ExportContainerSuccess) {
  crostini_manager()->ExportLxdContainer(
      container_id(), base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS, 123, 456));

  // Send signals, STREAMING, DONE.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ExportLxdContainer(
      container_id(), base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS, 123, 456));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ExportLxdContainer(
      container_id(), base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, base::DoNothing(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0));

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailFromSignal) {
  crostini_manager()->ExportLxdContainer(
      container_id(), base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 123, 456));

  // Send signal with FAILED.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ExportLxdContainer(
      container_id(), base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED,
                     0, 0));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerSuccess) {
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // Send signals, UPLOAD, UNPACK, DONE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(ExpectCrostiniResult, base::DoNothing(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailArchitecture) {
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(
          &ExpectCrostiniResult, run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE));

  // Send signal with FAILED_ARCHITECTURE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_FAILED_ARCHITECTURE);
  signal.set_architecture_device("archdev");
  signal.set_architecture_container("archcont");
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailFromSignal) {
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal with FAILED.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ImportLxdContainer(
      container_id(), base::FilePath("import_path"),
      base::BindOnce(
          &ExpectCrostiniResult, run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackageFromApt(
      container_id(), kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      container_id(), kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(
      "Unit tests can't install Linux package from apt!");
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      container_id(), kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      container_id(), kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallerStatusInitiallyFalse) {
  EXPECT_FALSE(
      crostini_manager()->GetCrostiniDialogStatus(DialogType::INSTALLER));
}

TEST_F(CrostiniManagerTest, StartContainerSuccess) {
  crostini_manager()->StartLxdContainer(
      container_id(),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StopContainerSuccess) {
  crostini_manager()->StopLxdContainer(
      container_id(),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, FileSystemCorruptionSignal) {
  base::HistogramTester histogram_tester{};

  anomaly_detector::GuestFileCorruptionSignal signal;
  fake_anomaly_detector_client_->NotifyGuestFileCorruption(signal);

  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::OTHER_CORRUPTION, 1);
}

TEST_F(CrostiniManagerTest, StartLxdSuccess) {
  crostini_manager()->StartLxd(
      kVmName, base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                              CrostiniResult::SUCCESS));

  run_loop()->Run();
}

class CrostiniManagerAnsibleInfraTest : public CrostiniManagerRestartTest {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();
    mock_ansible_management_service_ =
        AnsibleManagementTestHelper::SetUpMockAnsibleManagementService(
            profile_.get());
    ansible_management_test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    ansible_management_test_helper_->SetUpAnsibleInfra();
    SetUpViewsEnvironmentForTesting();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();

    TearDownViewsEnvironmentForTesting();

    ansible_management_test_helper_.reset();
    CrostiniManagerTest::TearDown();
  }

 protected:
  MockAnsibleManagementService* mock_ansible_management_service() {
    return mock_ansible_management_service_;
  }

  std::unique_ptr<AnsibleManagementTestHelper> ansible_management_test_helper_;
  MockAnsibleManagementService* mock_ansible_management_service_;
};

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerFailure) {
  EXPECT_CALL(*mock_ansible_management_service(), ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service(), ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& container_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(false);
      });

  CrostiniManager::RestartOptions ansible_restart;
  ansible_restart.ansible_playbook = profile_->GetPrefs()->GetFilePath(
      prefs::kCrostiniAnsiblePlaybookFilePath);

  crostini_manager()->RestartCrostiniWithOptions(
      DefaultContainerId(), std::move(ansible_restart),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_CONFIGURATION_FAILED),
      this);

  run_loop()->Run();
}

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerSuccess) {
  EXPECT_CALL(*mock_ansible_management_service(), ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service(), ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& container_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(true);
      });

  CrostiniManager::RestartOptions ansible_restart;
  ansible_restart.ansible_playbook = profile_->GetPrefs()->GetFilePath(
      prefs::kCrostiniAnsiblePlaybookFilePath);

  crostini_manager()->RestartCrostiniWithOptions(
      DefaultContainerId(), std::move(ansible_restart),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS),
      this);
  run_loop()->Run();
}

class CrostiniManagerUpgradeContainerTest
    : public CrostiniManagerTest,
      public UpgradeContainerProgressObserver {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();
    progress_signal_.set_owner_id(CryptohomeIdForProfile(profile()));
    progress_signal_.set_vm_name(kVmName);
    progress_signal_.set_container_name(kContainerName);
    progress_run_loop_ = std::make_unique<base::RunLoop>();
    crostini_manager()->AddUpgradeContainerProgressObserver(this);
  }

  void TearDown() override {
    crostini_manager()->RemoveUpgradeContainerProgressObserver(this);
    progress_run_loop_.reset();
    CrostiniManagerTest::TearDown();
  }

  void RunUntilUpgradeDone(UpgradeContainerProgressStatus final_status) {
    final_status_ = final_status;
    progress_run_loop_->Run();
  }

  void SendProgressSignal() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ash::FakeCiceroneClient::NotifyUpgradeContainerProgress,
                       base::Unretained(fake_cicerone_client_),
                       progress_signal_));
  }

 protected:
  // UpgradeContainerProgressObserver
  void OnUpgradeContainerProgress(
      const guest_os::GuestId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) override {
    if (status == final_status_) {
      progress_run_loop_->Quit();
    }
  }

  guest_os::GuestId container_id_ =
      guest_os::GuestId(kCrostiniDefaultVmType, kVmName, kContainerName);

  UpgradeContainerProgressStatus final_status_ =
      UpgradeContainerProgressStatus::FAILED;

  vm_tools::cicerone::UpgradeContainerProgressSignal progress_signal_;
  // must be created on UI thread
  std::unique_ptr<base::RunLoop> progress_run_loop_;
};

TEST_F(CrostiniManagerUpgradeContainerTest, UpgradeContainerSuccess) {
  crostini_manager()->UpgradeContainer(
      container_id_, ContainerVersion::BUSTER,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  run_loop()->Run();

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::SUCCEEDED);

  SendProgressSignal();
  RunUntilUpgradeDone(UpgradeContainerProgressStatus::SUCCEEDED);
}

TEST_F(CrostiniManagerUpgradeContainerTest, CancelUpgradeContainerSuccess) {
  crostini_manager()->UpgradeContainer(
      container_id_, ContainerVersion::BUSTER,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::IN_PROGRESS);

  SendProgressSignal();
  run_loop()->Run();

  base::RunLoop run_loop2;
  crostini_manager()->CancelUpgradeContainer(
      container_id_,
      base::BindOnce(&ExpectCrostiniResult, run_loop2.QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop2.Run();
}

}  // namespace crostini
