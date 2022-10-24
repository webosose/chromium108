// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from ash/public/cpp/notifier_settings_controller.cc

#include "neva/app_runtime/public/notifier_settings_controller.h"

#include "base/check_op.h"

namespace neva_app_runtime {

namespace {

NotifierSettingsController* g_instance = nullptr;

}  // namespace

// static
NotifierSettingsController* NotifierSettingsController::Get() {
  return g_instance;
}

NotifierSettingsController::NotifierSettingsController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

NotifierSettingsController::~NotifierSettingsController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace neva_app_runtime
