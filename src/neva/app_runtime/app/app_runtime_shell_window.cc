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

#include "neva/app_runtime/app/app_runtime_shell_window.h"

#include "neva/app_runtime/app/app_runtime_page_view.h"
#include "neva/app_runtime/app/app_runtime_shell_window_delegate.h"
#include "neva/app_runtime/ui/desktop_aura/app_runtime_desktop_native_widget_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace neva_app_runtime {

namespace {
const int kKeyboardHeightMargin = 10;
}

ShellWindow::ShellWindow(const CreateParams& params,
                         ShellWindowDelegate* delegate)
    : delegate_(delegate ? delegate : &stub_delegate_),
      height_(params.height) {
  Init(params);
}

ShellWindow::~ShellWindow() {
  if (delegate_)
    delegate_->OnDestroying(this);
}

void ShellWindow::SetWindowTitle(std::u16string title) {
  title_ = std::move(title);
}

void ShellWindow::SetDelegate(ShellWindowDelegate* delegate) {
  delegate_ = delegate ? delegate : &stub_delegate_;
}

PageView* ShellWindow::GetPageView() const {
  return page_view_.get();
}

std::unique_ptr<PageView> ShellWindow::SetPageView(
    std::unique_ptr<PageView> page_view) {
  auto previous_page_view = std::move(page_view_);
  page_view_ = std::move(page_view);
  page_view_->SetParentShellWindow(this);

  RemoveAllChildViews();
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(page_view_->GetView());
  Layout();
  if (previous_page_view.get())
    previous_page_view->SetParentShellWindow(nullptr);
  return previous_page_view;
}

std::unique_ptr<PageView> ShellWindow::RemovePageView() {
  RemoveChildView(page_view_->GetView());
  page_view_->SetParentShellWindow(nullptr);
  return std::move(page_view_);
}

void ShellWindow::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  if (changed_metrics &
      (display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
       display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_ROTATION)) {
    delegate_->OnDisplaySizeChanged(display.bounds());
  }
}

void ShellWindow::CursorVisibilityChanged(bool visible) {}

void ShellWindow::InputPanelVisibilityChanged(bool visible) {
  vkb_visible_state_ = visible;
  delegate_->VirtuaKeyboardChangeState(visible);
}

bool ShellWindow::IsTextInputOverlapped(gfx::Rect& input_panel_rect) {
  if (!host_)
    return false;

  ui::InputMethod* ime = host_->AsWindowTreeHost()->GetInputMethod();
  if (!ime || !ime->GetTextInputClient())
    return false;

  gfx::Rect input_bounds = ime->GetTextInputClient()->GetTextInputBounds();
  int input_bottom = input_bounds.y() + input_bounds.height();

  return height_ - input_bottom <
         input_panel_rect.height() + kKeyboardHeightMargin;
}

void ShellWindow::InputPanelRectChanged(int32_t x,
                                        int32_t y,
                                        uint32_t width,
                                        uint32_t height) {
  if (vkb_visible_state_) {
    auto bounds = gfx::Rect(x, y, width, height);
    if (IsTextInputOverlapped(bounds))
      delegate_->VirtuaKeyboardOverlapTextField(bounds);
  }
}

void ShellWindow::KeyboardEnter() {}

void ShellWindow::KeyboardLeave() {}

void ShellWindow::WindowHostClose() {}

void ShellWindow::WindowHostExposed() {}

void ShellWindow::WindowHostStateChanged(ui::WidgetState new_state) {}

void ShellWindow::WindowHostStateAboutToChange(ui::WidgetState state) {}

std::u16string ShellWindow::GetWindowTitle() const {
  return title_;
}

void ShellWindow::ToggleFullscreen() {
  auto* native_window = GetWidget()->GetNativeWindow();
  if (native_window && native_window->GetHost())
    native_window->GetHost()->ToggleFullscreen();
}

void ShellWindow::WindowClosing() {
  if (delegate_)
    delegate_->OnWindowClosing();
}

void ShellWindow::Init(const CreateParams& params) {
  // widget will be removed by its NativeWidget, or when closing with CloseNow.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams init_params(
      params.frameless ? views::Widget::InitParams::TYPE_WINDOW_FRAMELESS
                       : views::Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, params.width, params.height);
  // Since ShellWindow is WigetViewDelegate, it's set being owned by its widget.
  init_params.delegate = this;
  init_params.show_state = ui::SHOW_STATE_DEFAULT;

  display::Screen::GetScreen()->AddObserver(this);

  desktop_native_widget_aura_ = new AppRuntimeDesktopNativeWidgetAura(widget);
  desktop_native_widget_aura_->SetNativeEventDelegate(this);
  init_params.native_widget = desktop_native_widget_aura_;
  host_ =
      views::DesktopWindowTreeHost::Create(widget, desktop_native_widget_aura_);
  init_params.desktop_window_tree_host = host_;
  init_params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget->Init(std::move(init_params));
  host_->AsWindowTreeHost()->SetWindowProperty("appId", params.app_id);
  widget->Show();
}

}  // namespace neva_app_runtime
