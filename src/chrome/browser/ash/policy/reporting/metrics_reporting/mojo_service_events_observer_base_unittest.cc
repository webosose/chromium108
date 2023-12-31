// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::mojom::CrosHealthdAudioObserver;

class FakeCrosHealthdAudioObserver
    : public CrosHealthdAudioObserver,
      public MojoServiceEventsObserverBase<CrosHealthdAudioObserver> {
 public:
  FakeCrosHealthdAudioObserver()
      : MojoServiceEventsObserverBase<CrosHealthdAudioObserver>(this) {}

  FakeCrosHealthdAudioObserver(const FakeCrosHealthdAudioObserver&) = delete;
  FakeCrosHealthdAudioObserver& operator=(const FakeCrosHealthdAudioObserver&) =
      delete;

  ~FakeCrosHealthdAudioObserver() override = default;

  void OnUnderrun() override {
    MetricData metric_data;
    metric_data.mutable_telemetry_data();
    OnEventObserved(std::move(metric_data));
  }

  void OnSevereUnderrun() override {}

  void FlushForTesting() { receiver_.FlushForTesting(); }

 protected:
  void AddObserver() override {
    ash::cros_healthd::ServiceConnection::GetInstance()->AddAudioObserver(
        BindNewPipeAndPassRemote());
  }
};

class MojoServiceEventsObserverBaseTest : public ::testing::Test {
 public:
  MojoServiceEventsObserverBaseTest() = default;

  MojoServiceEventsObserverBaseTest(const MojoServiceEventsObserverBaseTest&) =
      delete;
  MojoServiceEventsObserverBaseTest& operator=(
      const MojoServiceEventsObserverBaseTest&) = delete;

  ~MojoServiceEventsObserverBaseTest() override = default;

  void SetUp() override { ::ash::cros_healthd::FakeCrosHealthd::Initialize(); }

  void TearDown() override { ::ash::cros_healthd::FakeCrosHealthd::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MojoServiceEventsObserverBaseTest, Default) {
  FakeCrosHealthdAudioObserver audio_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    result_metric_data = std::move(metric_data);
  });
  audio_observer.SetOnEventObservedCallback(std::move(cb));

  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(true);
    ::ash::cros_healthd::FakeCrosHealthd::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is enabled.
  ASSERT_TRUE(result_metric_data.has_telemetry_data());

  // Shutdown cros_healthd to simulate crash.
  ash::cros_healthd::FakeCrosHealthd::Shutdown();
  // Restart cros_healthd.
  ash::cros_healthd::FakeCrosHealthd::Initialize();
  audio_observer.FlushForTesting();

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    ::ash::cros_healthd::FakeCrosHealthd::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Observer reconnected after crash.
  EXPECT_TRUE(result_metric_data.has_telemetry_data());

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(false);
    ::ash::cros_healthd::FakeCrosHealthd::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is disabled.
  EXPECT_FALSE(result_metric_data.has_telemetry_data());
}

}  // namespace
}  // namespace reporting
