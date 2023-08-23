// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

#include <memory>

#include "build/build_config.h"
#include "ui/aura/screen_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

///@name USE_NEVA_APPRUNTIME
///@{
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"
///@}

namespace views {

DesktopScreenOzone::DesktopScreenOzone() = default;

DesktopScreenOzone::~DesktopScreenOzone() = default;

gfx::NativeWindow DesktopScreenOzone::GetNativeWindowFromAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  ///@name USE_NEVA_APPRUNTIME
  ///@{
  if (ui::OzonePlatform::IsWaylandExternal())
    return nullptr;
  ///@}

  if (!widget)
    return nullptr;
  return views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
      widget);
}

#if !BUILDFLAG(IS_LINUX)
std::unique_ptr<display::Screen> CreateDesktopScreen() {
  ///@name USE_NEVA_APPRUNTIME
  ///@{
  if (ui::OzonePlatform::IsWaylandExternal())
      return std::unique_ptr<display::Screen>(
          DesktopFactoryOzone::GetInstance()->CreateDesktopScreen());
  ///@}
  auto screen = std::make_unique<DesktopScreenOzone>();
  screen->Initialize();
  return screen;
}
#endif

}  // namespace views
