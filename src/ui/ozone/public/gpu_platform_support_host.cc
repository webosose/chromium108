// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "base/trace_event/trace_event.h"

namespace ui {

namespace {

// No-op implementations of GpuPlatformSupportHost.
class StubGpuPlatformSupportHost : public GpuPlatformSupportHost {
 public:
  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override {}
  void OnGpuServiceLaunched(
      int host_id,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override {}
};

}  // namespace

GpuPlatformSupportHost::GpuPlatformSupportHost() {
}

GpuPlatformSupportHost::~GpuPlatformSupportHost() {
}

GpuPlatformSupportHost* CreateStubGpuPlatformSupportHost() {
  return new StubGpuPlatformSupportHost;
}

#if defined(USE_OZONE) && defined(OZONE_PLATFORM_WAYLAND_EXTERNAL)
void GpuPlatformSupportHost::OnGpuProcessLaunched(
    int host_id,
    base::RepeatingCallback<void(IPC::Message*)> sender) {}
void GpuPlatformSupportHost::OnMessageReceived(const IPC::Message& message) {}
#endif

}  // namespace ui
