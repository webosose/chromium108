// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view.h"

#include <algorithm>
#include <numeric>

#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The default padding within each grid in grid layout.
constexpr int kGridInnerPadding = 5;
// The default spacing between the row groups in grid layout.
constexpr int kGridRowGroupSpacing = 20;
// The default spacing between the column groups in grid layout.
constexpr int kGridColGroupSpacing = 20;
// The default insets of the grid layout border.
constexpr gfx::Insets kGridBorderInsets(10);

}  // namespace

//------------------------------------------------------------------------------
// SystemUIComponentsGridView::GridLayout:
// GridLayout splits the contents into `row_num` x `col_num` grids. Grids in the
// same row have same height and grids in the same column have same width. The
// rows and columns can be divided into equal sized groups. `row_group_size` and
// `col_group_size` indicate the number of rows and columns in each row and
// column group. If the number of rows and columns cannot be divided by their
// group size, the last group will have the remainders. There is a spacing
// between the row and column groups. There is also a border around the grids.
// An example is shown below:
// +---------------------------------------------------------------------------+
// |                                border                                     |
// |        +--------+-------+                   +---------+----------+        |
// |        |        |       | col group spacing |         |          |        |
// |        +--------+-------+                   +---------+----------+        |
// |         row group spacing                     row group spacing           |
// |        +--------+-------+                   +---------+----------+        |
// | border |        |       |                   |         |          | border |
// |        |        |       | col group spacing |         |          |        |
// |        +--------+-------+                   +---------+----------+        |
// |                                border                                     |
// +---------------------------------------------------------------------------+

class SystemUIComponentsGridView::GridLayout : public views::LayoutManager {
 public:
  GridLayout(size_t row_num,
             size_t col_num,
             size_t row_group_size,
             size_t col_group_size,
             int inner_padding,
             int row_group_spacing,
             int col_group_spacing,
             const gfx::Insets& border_insets)
      : row_num_(row_num),
        col_num_(col_num),
        col_width_(col_num),
        row_height_(row_num),
        row_group_size_(row_group_size),
        col_group_size_(col_group_size),
        inner_padding_(inner_padding),
        row_group_spacing_(row_group_spacing),
        col_group_spacing_(col_group_spacing),
        border_insets_(border_insets) {
    // Clamp the row and column group size between 1 and the number of rows and
    // columns when the layout is not empty.
    if (row_num_ > 0 && col_num_ > 0) {
      row_group_size_ =
          std::clamp(row_group_size, static_cast<size_t>(1), row_num_);
      col_group_size_ =
          std::clamp(col_group_size, static_cast<size_t>(1), col_num_);
    }
  }
  GridLayout(const GridLayout&) = delete;
  GridLayout& operator=(const GridLayout&) = delete;
  ~GridLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    // No layout if either row/column is empty.
    if (row_num_ == 0 || col_num_ == 0)
      return;

    // The x of grids origin in different columns.
    std::vector<int> ori_x(col_num_, 0);
    // The y of grids origin in different rows.
    std::vector<int> ori_y(row_num_, 0);
    const std::vector<views::View*>& children = host->children();

    ori_x[0] = border_insets_.left();
    ori_y[0] = border_insets_.top();
    for (size_t i = 0; i < children.size(); i++) {
      int row_i = i / col_num_;
      int col_i = i % col_num_;
      // Calculate the origin posisitons.
      if (row_i == 0 && col_i > 0) {
        int col_padding = (col_i % col_group_size_) ? 0 : col_group_spacing_;
        ori_x[col_i] = ori_x[col_i - 1] + col_width_[col_i - 1] + col_padding;
      }
      if (row_i > 0 && col_i == 0) {
        int row_padding = (row_i % row_group_size_) ? 0 : row_group_spacing_;
        ori_y[row_i] = ori_y[row_i - 1] + row_height_[row_i - 1] + row_padding;
      }

      // Put the view in the center of the grid.
      int view_width = children[i]->GetPreferredSize().width();
      int view_height = children[i]->GetPreferredSize().height();
      children[i]->SetBoundsRect(
          gfx::Rect(ori_x[col_i] + inner_padding_,
                    ori_y[row_i] + (row_height_[row_i] - view_height) / 2,
                    view_width, view_height));
    }
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    // Size = (0, 0) if either row or column is empty.
    if (row_num_ == 0 || col_num_ == 0)
      return gfx::Size();

    // Preferred Size = Grid Size + Total Spacing + Border Size.
    int width = std::accumulate(col_width_.begin(), col_width_.end(), 0) +
                (col_num_ - 1) / col_group_size_ * col_group_spacing_ +
                border_insets_.width();
    int height = std::accumulate(row_height_.begin(), row_height_.end(), 0) +
                 (row_num_ - 1) / row_group_size_ * row_group_spacing_ +
                 border_insets_.height();
    return gfx::Size(width, height);
  }

  void ViewAdded(views::View* host, views::View* view) override {
    // Number of children cannot exceed the layout capacity.
    DCHECK_LE(host->children().size(), row_num_ * col_num_);
    ChildViewSizeChanged(host, view);
  }

  void ChildPreferredSizeChanged(views::View* host, views::View* view) {
    ChildViewSizeChanged(host, view);
  }

 private:
  // Called when the size of a `view` in the `host` children changed.
  void ChildViewSizeChanged(views::View* host, views::View* view) {
    // Get the index of `view` in `host` children.
    const std::vector<views::View*>& children = host->children();
    auto iter = std::find(children.begin(), children.end(), view);
    DCHECK(iter != children.end());
    const int index = std::distance(children.begin(), iter);

    // When a view size is changed, updates the max width of the column and max
    // height of the row.
    int row_i = index / col_num_;
    int col_i = index % col_num_;
    gfx::Size view_size = view->GetPreferredSize();

    row_height_[row_i] =
        std::max(row_height_[row_i], view_size.height() + 2 * inner_padding_);
    col_width_[col_i] =
        std::max(col_width_[col_i], view_size.width() + 2 * inner_padding_);

    // Re-layout the host view.
    Layout(host);
  }

  // The number of rows and columns.
  const size_t row_num_;
  const size_t col_num_;
  // The width of different columns.
  std::vector<int> col_width_;
  // The height of different rows.
  std::vector<int> row_height_;
  // The size of each row and column group.
  size_t row_group_size_;
  size_t col_group_size_;
  // The padding in each grid.
  int inner_padding_;
  // Spacing between row groups.
  int row_group_spacing_;
  // Spacing between column groups.
  int col_group_spacing_;
  gfx::Insets border_insets_;
};

// -----------------------------------------------------------------------------
// SystemUIComponentsGridView:
// We assume each column in the contents view at least has a label and
// an instance. Therefore, the column of contents view occupies two columns of
// the grid layout.
SystemUIComponentsGridView::SystemUIComponentsGridView(size_t row_num,
                                                       size_t col_num,
                                                       size_t row_group_size,
                                                       size_t col_group_size)
    : grid_layout_(
          SetLayoutManager(std::make_unique<GridLayout>(row_num,
                                                        2 * col_num,
                                                        row_group_size,
                                                        2 * col_group_size,
                                                        kGridInnerPadding,
                                                        kGridRowGroupSpacing,
                                                        kGridColGroupSpacing,
                                                        kGridBorderInsets))) {}

SystemUIComponentsGridView::~SystemUIComponentsGridView() = default;

void SystemUIComponentsGridView::ChildPreferredSizeChanged(views::View* child) {
  // Update the layout when a child size is changed.
  grid_layout_->ChildPreferredSizeChanged(this, child);
}

void SystemUIComponentsGridView::AddInstanceImpl(const std::u16string& name,
                                                 views::View* instance_view) {
  // Add a label and an instance in the contents.
  AddChildView(std::make_unique<views::Label>(name));
  AddChildView(instance_view);
}

}  // namespace ash
