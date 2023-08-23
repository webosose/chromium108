// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Reused from chrome/browser/media/media_access_handler.cc

#include "neva/app_runtime/browser/media/media_access_handler.h"

namespace neva_app_runtime {

bool MediaAccessHandler::IsInsecureCapturingInProgress(int render_process_id,
                                                       int render_frame_id) {
  return false;
}

}  // namespace neva_app_runtime
