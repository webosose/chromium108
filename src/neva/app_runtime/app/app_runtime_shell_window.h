// Copyright 2021 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_WINDOW_H_
#define NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_WINDOW_H_

#include <memory>
#include <string>

#include "neva/app_runtime/app/app_runtime_shell_window_delegate.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/desktop_aura/neva/native_event_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace display {
class Display;
}  // namespace display

namespace neva_app_runtime {

class PageView;
class AppRuntimeDesktopNativeWidgetAura;

class ShellWindow : public views::NativeEventDelegate,
                    public views::WidgetDelegateView,
                    public display::DisplayObserver {
 public:
  struct CreateParams {
    int width = 0;
    int height = 0;
    bool frameless = false;
    std::string app_id;
  };

  ShellWindow(const CreateParams& params,
              ShellWindowDelegate* delegate = nullptr);
  ShellWindow(const ShellWindow&) = delete;
  ShellWindow& operator=(const ShellWindow&) = delete;
  ~ShellWindow() override;

  void SetDelegate(ShellWindowDelegate* delegate);

  PageView* GetPageView() const;
  std::unique_ptr<PageView> SetPageView(std::unique_ptr<PageView> page_view);
  std::unique_ptr<PageView> RemovePageView();

  void SetWindowTitle(std::u16string title);
  void ToggleFullscreen();

  // ui::DisplayObserver
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // views::NativeEventDelegate
  void CursorVisibilityChanged(bool visible) override;
  void InputPanelVisibilityChanged(bool visible) override;
  void InputPanelRectChanged(int32_t x,
                             int32_t y,
                             uint32_t width,
                             uint32_t height) override;
  void KeyboardEnter() override;
  void KeyboardLeave() override;
  void WindowHostClose() override;
  void WindowHostExposed() override;
  void WindowHostStateChanged(ui::WidgetState new_state) override;
  void WindowHostStateAboutToChange(ui::WidgetState state) override;

  // views::WidgetDelegateView
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 private:
  void Init(const CreateParams& params);
  bool IsTextInputOverlapped(gfx::Rect& input_panel_rect);
  std::u16string title_;
  ShellWindowDelegate* delegate_;
  ShellWindowDelegate stub_delegate_;
  std::unique_ptr<PageView> page_view_;
  AppRuntimeDesktopNativeWidgetAura* desktop_native_widget_aura_;
  bool vkb_visible_state_ = false;
  views::DesktopWindowTreeHost* host_ = nullptr;
  int height_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_APP_APP_RUNTIME_SHELL_WINDOW_H_
