// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OZONE_WEB_UI_H_
#define CHROME_BROWSER_UI_OZONE_WEB_UI_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "build/buildflag.h"
#include "ozone/platform/ozone_export_wayland.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/image/image.h"
#include "ui/linux/linux_ui.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/window/frame_buttons.h"

class SkBitmap;

namespace gfx {
class Image;
}

namespace ui {
class OzoneGpuPlatformSupportHost;
class InputMethodContextManager;
class NavButtonProvider;
}

namespace views {
class View;

// Interface to Wayland desktop features.
//
class OZONE_WAYLAND_EXPORT OzoneWebUI : public ui::LinuxUi {
 public:
  OzoneWebUI();
  OzoneWebUI(const OzoneWebUI&) = delete;
  OzoneWebUI& operator=(const OzoneWebUI&) = delete;
  ~OzoneWebUI() override;

  // ui::LinuxShellDialog:
  ui::SelectFileDialog* CreateSelectFileDialog(
      void* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;

#if BUILDFLAG(ENABLE_PRINTING)
  // printing::PrintingContextLinuxDelegate:
  printing::PrintDialogLinuxInterface* CreatePrintDialog(
      printing::PrintingContextLinux* context) override;
  gfx::Size GetPdfPaperSize(printing::PrintingContextLinux* context) override;
#endif

  // ui::LinuxUi:
  bool Initialize() override;
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;

  // These methods are not needed
  base::TimeDelta GetCursorBlinkInterval() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  bool GetTextEditCommandsForEvent(
      const ui::Event& event,
      std::vector<ui::TextEditCommandAuraLinux>* commands) override;
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      int* weight_out,
      gfx::FontRenderParams* params_out) const override;

  float GetDeviceScaleFactor() const override;

  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

  // ui::CursorThemeManagerLinux
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;

  // gfx::AnimationSettingsProviderLinux
  bool AnimationsEnabled() const override;

 private:
  std::unique_ptr<ui::InputMethodContextManager> input_method_context_manager_;
  ui::OzoneGpuPlatformSupportHost* host_;
};

}  // namespace views

ui::LinuxUi* BuildWebUI();

#endif  // CHROME_BROWSER_UI_OZONE_WEB_UI_H_
