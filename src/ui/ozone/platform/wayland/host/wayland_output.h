// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_

#include <cstdint>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class XDGOutput;
class WaylandZcrColorManager;
class WaylandZcrColorManagementOutput;
class WaylandConnection;
class WaylandZAuraOutput;

// WaylandOutput objects keep track of wl_output information received through
// the Wayland protocol, along with other related protocol extensions, such as,
// xdg-output and ChromeOS's aura-shell.
class WaylandOutput : public wl::GlobalObjectRegistrar<WaylandOutput> {
 public:
  // Instances of this class are identified by an 32-bit unsigned int value,
  // corresponding to its global wl_output object 'name' value. It is mostly
  // used interchangeably with WaylandScreen's display::Display::id property,
  // which is an int64_t instead, though it is worth bearing in mind they are
  // slightly different, under the hood.
  using Id = uint32_t;

  static constexpr char kInterfaceName[] = "wl_output";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  class Delegate {
   public:
    virtual void OnOutputHandleMetrics(Id output_id,
                                       const gfx::Point& origin,
                                       const gfx::Size& logical_size,
                                       const gfx::Size& physical_size,
                                       const gfx::Insets& insets,
                                       float scale_factor,
                                       int32_t panel_transform,
                                       int32_t logical_transform,
                                       const std::string& description) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  WaylandOutput(Id output_id, wl_output* output, WaylandConnection* connection);

  WaylandOutput(const WaylandOutput&) = delete;
  WaylandOutput& operator=(const WaylandOutput&) = delete;

  ~WaylandOutput();

  void Initialize(Delegate* delegate);
  void InitializeXdgOutput(struct zxdg_output_manager_v1* manager);
  void InitializeZAuraOutput(zaura_shell* aura_shell);
  void InitializeColorManagementOutput(WaylandZcrColorManager* manager);
  float GetUIScaleFactor() const;

  ///@name USE_NEVA_APPRUNTIME
  ///@{
  wl_output* output() const { return output_.get(); }
  ///@}
  Id output_id() const { return output_id_; }
  bool has_output(wl_output* output) const { return output_.get() == output; }
  float scale_factor() const { return scale_factor_; }
  int32_t panel_transform() const { return panel_transform_; }
  int32_t logical_transform() const;
  gfx::Point origin() const;
  gfx::Size logical_size() const;
  gfx::Size physical_size() const { return physical_size_; }
  gfx::Insets insets() const;
  const std::string& name() const;
  const std::string& description() const;
  WaylandZcrColorManagementOutput* color_management_output() const {
    return color_management_output_.get();
  }

  // Tells if the output has already received physical screen dimensions in the
  // global compositor space.
  bool is_ready() const { return !physical_size_.IsEmpty(); }

  wl_output* get_output() { return output_.get(); }
  zaura_output* get_zaura_output();

  void SetScaleFactorForTesting(float scale_factor);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, NameAndDescriptionFallback);

  static constexpr int32_t kDefaultScaleFactor = 1;

  void TriggerDelegateNotifications();

  // Callback functions used for setting geometric properties of the output
  // and available modes.
  static void OutputHandleGeometry(void* data,
                                   wl_output* output,
                                   int32_t x,
                                   int32_t y,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t subpixel,
                                   const char* make,
                                   const char* model,
                                   int32_t output_transform);

  static void OutputHandleMode(void* data,
                               wl_output* wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh);
  static void OutputHandleDone(void* data, struct wl_output* wl_output);
  static void OutputHandleScale(void* data,
                                struct wl_output* wl_output,
                                int32_t factor);
#if CHROME_WAYLAND_CHECK_VERSION(1, 20, 0)
  static void OutputHandleName(void* data,
                               struct wl_output* wl_output,
                               const char* name);
  static void OutputHandleDescription(void* data,
                                      struct wl_output* wl_output,
                                      const char* description);
#endif

  const Id output_id_ = 0;
  wl::Object<wl_output> output_;
  std::unique_ptr<XDGOutput> xdg_output_;
  std::unique_ptr<WaylandZAuraOutput> aura_output_;
  std::unique_ptr<WaylandZcrColorManagementOutput> color_management_output_;
  float scale_factor_ = kDefaultScaleFactor;
  int32_t panel_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  // Origin of the output in DIP screen coordinate.
  gfx::Point origin_;
  // Size of the output in physical pixels.
  gfx::Size physical_size_;

  // Fallback name and description.
  // The XDG output specification suggests using it as the primary source of
  // the information about the output.  Two attributes below are used if
  // xdg_output_ is not present.
  // See https://wayland.app/protocols/xdg-output-unstable-v1
  std::string name_;
  std::string description_;

  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<WaylandConnection> connection_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_
