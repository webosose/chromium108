// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
#define ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_

#include "ash/test/ash_pixel_diff_util.h"
#include "base/check_op.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {
class DemoAshPixelDiffTest;

// A helper class that provides utility functions for performing pixel diff
// tests via the Skia Gold.
class AshPixelDiffTestHelper {
 public:
  // `screenshot_prefix` is the prefix of the screenshot names; `corpus`
  // specifies the result group that will be used to store screenshots in Skia
  // Gold. Read the comment of `SKiaGoldPixelDiff::Init()` for more details.
  AshPixelDiffTestHelper(const std::string& screenshot_prefix,
                         const std::string& corpus);
  AshPixelDiffTestHelper(const AshPixelDiffTestHelper&) = delete;
  AshPixelDiffTestHelper& operator=(const AshPixelDiffTestHelper&) = delete;
  ~AshPixelDiffTestHelper();

  // Similar to `ComparePrimaryFullScreen()` but with the difference that only
  // the pixels within the screen bounds of `ui_components` are compared. The
  // function caller has the duty to choose suitable `ui_components` in their
  // tests to avoid unnecessary pixel comparisons. Otherwise, pixel tests could
  // be fragile to the changes in production code.
  // `ui_components` is a variadic argument list, consisting of view pointers,
  // widget pointers or window pointers. `ui_components` can have the pointers
  // of different categories.
  // Example usages:
  //
  //  views::View* view_ptr = ...;
  //  views::Widget* widget_ptr = ...;
  //  aura::Window* window_ptr = ...;
  //
  //  CompareUiComponentsOnPrimaryScreen("foo_name1", view_ptr);
  //
  //  CompareUiComponentsOnPrimaryScreen("foo_name2",
  //                                     view_ptr,
  //                                     widget_ptr,
  //                                     window_ptr);
  template <class... UiComponentTypes>
  bool CompareUiComponentsOnPrimaryScreen(const std::string& screenshot_name,
                                          UiComponentTypes... ui_components) {
    DCHECK_GT(sizeof...(ui_components), 0u);
    std::vector<gfx::Rect> rects_in_screen;
    PopulateUiComponentScreenBounds(&rects_in_screen, ui_components...);
    return ComparePrimaryScreenshotInRects(screenshot_name, rects_in_screen);
  }

 private:
  friend DemoAshPixelDiffTest;

  // Takes a screenshot of the primary fullscreen then uploads it to the Skia
  // Gold to perform pixel comparison. `screenshot_name` specifies the benchmark
  // image. Returns the comparison result.
  // NOTE: use this function only when necessary. Otherwise, a tiny UI change
  // may break many pixel tests.
  bool ComparePrimaryFullScreen(const std::string& screenshot_name);

  // Compares a screenshot of the primary screen with the specified benchmark
  // image. Only the pixels in `rects_in_screen` affect the comparison result.
  bool ComparePrimaryScreenshotInRects(
      const std::string& screenshot_name,
      const std::vector<gfx::Rect>& rects_in_screen);

  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  // NOTE: the user of `ViewSkiaGoldPixelDiff` has the duty to initialize
  // `pixel_diff` before performing any pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
