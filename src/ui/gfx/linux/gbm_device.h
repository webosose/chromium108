// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_DEVICE_H_
#define UI_GFX_LINUX_GBM_DEVICE_H_

#if !defined(USE_NEVA_V4L2_CODEC)
#include <gbm.h>
#endif
#include <memory>

#include "base/files/file.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

class GbmBuffer;

class GbmDevice {
 public:
  virtual ~GbmDevice() {}

  virtual std::unique_ptr<GbmBuffer> CreateBuffer(uint32_t format,
                                                  const gfx::Size& size,
                                                  uint32_t flags) = 0;
  virtual std::unique_ptr<GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) = 0;
  virtual std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      uint32_t format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) = 0;

  virtual bool CanCreateBufferForFormat(uint32_t format) = 0;
};

}  // namespace ui

#endif  // UI_GFX_LINUX_GBM_DEVICE_H_
