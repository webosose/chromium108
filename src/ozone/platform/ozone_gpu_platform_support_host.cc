// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/platform/channel_observer.h"
#include "ozone/platform/ozone_gpu_platform_support_host.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"

namespace ui {

OzoneGpuPlatformSupportHost::OzoneGpuPlatformSupportHost() = default;
OzoneGpuPlatformSupportHost::~OzoneGpuPlatformSupportHost() = default;

void OzoneGpuPlatformSupportHost::RegisterHandler(
    GpuPlatformSupportHost* handler) {
  handlers_.push_back(handler);

  if (IsConnected())
    handler->OnGpuProcessLaunched(host_id_, send_callback_);
}

void OzoneGpuPlatformSupportHost::UnregisterHandler(
    GpuPlatformSupportHost* handler) {
  std::vector<GpuPlatformSupportHost*>::iterator it =
      std::find(handlers_.begin(), handlers_.end(), handler);
  if (it != handlers_.end())
    handlers_.erase(it);
}

void OzoneGpuPlatformSupportHost::AddChannelObserver(
    ChannelObserver* observer) {
  channel_observers_.AddObserver(observer);

  if (IsConnected())
    observer->OnGpuProcessLaunched();
}

void OzoneGpuPlatformSupportHost::RemoveChannelObserver(
    ChannelObserver* observer) {
  channel_observers_.RemoveObserver(observer);
}

bool OzoneGpuPlatformSupportHost::IsConnected() {
  return host_id_ >= 0 && gpu_process_launched_;
}

void OzoneGpuPlatformSupportHost::OnGpuProcessLaunched(
    int host_id,
    base::RepeatingCallback<void(IPC::Message*)> sender) {
  TRACE_EVENT1("drm", "OzoneGpuPlatformSupportHost::OnGpuProcessLaunched",
               "host_id", host_id);
  gpu_process_launched_ = true;
  host_id_ = host_id;
  send_callback_ = sender;

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnGpuProcessLaunched(host_id, send_callback_);

  for (auto& observer : channel_observers_)
    observer.OnGpuProcessLaunched();
}

void OzoneGpuPlatformSupportHost::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("drm", "OzoneGpuPlatformSupportHost::OnChannelDestroyed",
               "host_id", host_id);
  gpu_process_launched_ = false;
  if (host_id_ == host_id) {
    host_id_ = -1;
    send_callback_.Reset();
    for (auto& observer : channel_observers_)
      observer.OnChannelDestroyed();
  }

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelDestroyed(host_id);
}

void OzoneGpuPlatformSupportHost::OnGpuServiceLaunched(
    int host_id,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnGpuServiceLaunched(host_id, binder, terminate_callback);
}

void OzoneGpuPlatformSupportHost::OnMessageReceived(
    const IPC::Message& message) {
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnMessageReceived(message);
}

bool OzoneGpuPlatformSupportHost::Send(IPC::Message* message) {
  if (IsConnected()) {
    send_callback_.Run(message);
    return true;
  }
  delete message;
  return false;
}

}  // namespace ui
