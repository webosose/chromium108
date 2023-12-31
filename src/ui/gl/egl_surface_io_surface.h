// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_EGL_SURFACE_IO_SURFACE_H_
#define UI_GL_EGL_SURFACE_IO_SURFACE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <IOSurface/IOSurface.h>
#include <memory>

#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Helper class to create an EGLSurface (PBuffer) for an IOSurface and bind
// it to an EGL texture.
class GL_EXPORT ScopedEGLSurfaceIOSurface {
 public:
  static std::unique_ptr<ScopedEGLSurfaceIOSurface> Create(
      EGLDisplay display,
      IOSurfaceRef io_surface,
      uint32_t plane,
      gfx::BufferFormat format);
  ~ScopedEGLSurfaceIOSurface();

  bool BindTexImage(unsigned target);
  void ReleaseTexImage();

 private:
  explicit ScopedEGLSurfaceIOSurface(EGLDisplay display);
  bool CreatePBuffer(IOSurfaceRef io_surface,
                     uint32_t plane,
                     gfx::BufferFormat format);
  void DestroyPBuffer();

  EGLDisplay display_ = nullptr;
  EGLConfig dummy_config_ = EGL_NO_CONFIG_KHR;
  EGLint texture_target_ = EGL_NO_TEXTURE;
  EGLSurface pbuffer_ = EGL_NO_SURFACE;
  bool texture_bound_ = false;
};

}  // namespace gl

#endif  // UI_GL_EGL_SURFACE_IO_SURFACE_H_
