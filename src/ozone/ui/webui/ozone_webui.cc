// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/ui/webui/ozone_webui.h"

#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "ozone/platform/ozone_gpu_platform_support_host.h"
#include "ozone/platform/ozone_platform_wayland.h"
#include "ozone/ui/webui/input_method_context_impl_wayland.h"
#include "ozone/ui/webui/input_method_context_manager.h"
#include "ozone/ui/webui/select_file_dialog_impl_webui.h"
#include "ui/base/ime/linux/input_method_auralinux.h"
#include "ui/linux/nav_button_provider.h"

namespace views {

OzoneWebUI::OzoneWebUI() : host_(NULL) {
}

OzoneWebUI::~OzoneWebUI() {
}

bool OzoneWebUI::Initialize() {
  // TODO(kalyan): This is a hack, get rid  of this.
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  host_ = static_cast<ui::OzoneGpuPlatformSupportHost*>(
      platform->GetGpuPlatformSupportHost());
  input_method_context_manager_.reset(new ui::InputMethodContextManager(host_));
  return true;
}

ui::SelectFileDialog* OzoneWebUI::CreateSelectFileDialog(
    void* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
#if defined(USE_SELECT_FILE_DIALOG_WEBUI_IMPL)
  return ui::SelectFileDialogImplWebUI::Create(
      static_cast<ui::SelectFileDialog::Listener*>(listener), policy.get());
#endif
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<ui::LinuxInputMethodContext>
OzoneWebUI::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  DCHECK(input_method_context_manager_);
  unsigned handle = 0;
  ui::InputMethodAuraLinux* delegate_impl =
      static_cast<ui::InputMethodAuraLinux*>(delegate);
  if (delegate_impl)
    handle = delegate_impl->GetAcceleratedWndHandle();

  return std::unique_ptr<ui::LinuxInputMethodContext>(
      new ui::InputMethodContextImplWayland(
          delegate, handle, input_method_context_manager_.get()));
}

#if BUILDFLAG(ENABLE_PRINTING)
printing::PrintDialogLinuxInterface* OzoneWebUI::CreatePrintDialog(
    printing::PrintingContextLinux* context) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

gfx::Size OzoneWebUI::GetPdfPaperSize(printing::PrintingContextLinux* context) {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Size();
}
#endif

base::TimeDelta OzoneWebUI::GetCursorBlinkInterval() const {
  static const double kCursorBlinkTime = 1.0;
  return base::Seconds(kCursorBlinkTime);
}

gfx::Image OzoneWebUI::GetIconForContentType(const std::string& content_type,
                                             int size,
                                             float scale) const {
  return gfx::Image();
}

bool OzoneWebUI::GetTextEditCommandsForEvent(
    const ui::Event& event,
    std::vector<ui::TextEditCommandAuraLinux>* commands) {
  return false;
}

gfx::FontRenderParams OzoneWebUI::GetDefaultFontRenderParams() const {
  return gfx::FontRenderParams();
}

void OzoneWebUI::GetDefaultFontDescription(
    std::string* family_out,
    int* size_pixels_out,
    int* style_out,
    int* weight_out,
    gfx::FontRenderParams* params_out) const {
}

float OzoneWebUI::GetDeviceScaleFactor() const {
  return 0.0;
}

base::flat_map<std::string, std::string>
OzoneWebUI::GetKeyboardLayoutMap() {
  return {};
}

std::string OzoneWebUI::GetCursorThemeName() {
  return {};
}

int OzoneWebUI::GetCursorThemeSize() {
  return 0;
}

bool OzoneWebUI::AnimationsEnabled() const {
  return true;
}

}  // namespace views

ui::LinuxUi* BuildWebUI() {
  return new views::OzoneWebUI();
}
