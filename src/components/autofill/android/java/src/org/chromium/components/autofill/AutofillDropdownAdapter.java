// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.view.ViewCompat;

import org.chromium.ui.DropdownDividerDrawable;
import org.chromium.ui.DropdownItem;

import java.util.List;
import java.util.Set;

/**
 * Dropdown item adapter for the AutofillPopup.
 */
public class AutofillDropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;
    private final int mLabelMargin;
    private final boolean mIsRefresh;

    /**
     * Creates an {@code ArrayAdapter} with specified parameters.
     * @param context Application context.
     * @param items List of labels and icons to display.
     * @param separators Set of positions that separate {@code items}.
     * @param isRefresh Whether or not the dropdown should be presented using refreshed styling.
     */
    public AutofillDropdownAdapter(Context context, List<? extends DropdownItem> items,
            Set<Integer> separators, boolean isRefresh) {
        super(context,
                isRefresh ? R.layout.autofill_dropdown_item_refresh
                          : R.layout.autofill_dropdown_item);
        mContext = context;
        addAll(items);
        mSeparators = separators;
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
        mLabelMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_dropdown_item_label_margin);
        mIsRefresh = isRefresh;
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = getItem(i);
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            int layoutId = mIsRefresh ? R.layout.autofill_dropdown_item_refresh
                                      : R.layout.autofill_dropdown_item;
            layout = inflater.inflate(layoutId, null);
            layout.setBackground(new DropdownDividerDrawable(/*backgroundColor=*/null));
        }

        DropdownItem item = getItem(position);

        if (mIsRefresh) {
            TextView labelView = populateLabelView(item, layout);
            populateSecondaryLabelView(item, layout);
            populateSublabelView(item, layout);
            // For refreshed layout, ignore the return value as we don't need to adjust the height
            // of the view.
            populateItemTagView(item, layout);
            // Set the visibility of the start/end icons.
            layout.findViewById(R.id.start_dropdown_icon)
                    .setVisibility(item.isIconAtStart() ? View.VISIBLE : View.GONE);
            layout.findViewById(R.id.end_dropdown_icon)
                    .setVisibility(item.isIconAtStart() ? View.GONE : View.VISIBLE);
            ImageView iconView =
                    populateIconView((ImageView) (item.isIconAtStart()
                                                     ? layout.findViewById(R.id.start_dropdown_icon)
                                                     : layout.findViewById(R.id.end_dropdown_icon)),
                            item);
            if (iconView != null) {
                iconView.setLayoutParams(getSizeParamsForIconView(iconView, item));
            }

            if (item.isMultilineLabel()) {
                labelView.setSingleLine(false);

                LinearLayout wrapper =
                        (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);

                int paddingHeight = mContext.getResources().getDimensionPixelSize(
                        R.dimen.autofill_dropdown_refresh_vertical_padding);

                wrapper.setPadding(
                        /*left=*/0, /*top=*/paddingHeight, /*right=*/0, /*bottom=*/paddingHeight);
            }

            return layout;
        }

        // TODO(crbug.com/874077): The rest of this function builds the deprecated legacy UI.
        // Remove this branch once the refreshed UI is 100% rolled out.

        int height = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_dropdown_item_height);

        DropdownDividerDrawable divider = (DropdownDividerDrawable) layout.getBackground();
        if (position == 0) {
            divider.setDividerColor(Color.TRANSPARENT);
        } else {
            int dividerHeight = mContext.getResources().getDimensionPixelSize(
                    R.dimen.autofill_dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            int dividerColor;
            if (mSeparators != null && mSeparators.contains(position)) {
                dividerColor = mContext.getColor(R.color.dropdown_dark_divider_color);
            } else {
                dividerColor = mContext.getColor(R.color.dropdown_divider_color);
            }
            divider.setDividerColor(dividerColor);
        }

        // Layout of the item tag view, which has a smaller font and sits below the sub
        // label.
        TextView itemTagView = populateItemTagView(item, layout);
        if (itemTagView != null) {
            itemTagView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
            height += mContext.getResources().getDimensionPixelSize(
                    R.dimen.autofill_dropdown_item_tag_height);
        }

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews
        // instead. If you need to modify this layout, don't forget to test it with TalkBack and
        // make sure it doesn't regress. http://crbug.com/429364
        LinearLayout wrapper = (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);
        if (item.isMultilineLabel()) height = LayoutParams.WRAP_CONTENT;
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        // Layout of the main label view.
        TextView labelView = populateLabelView(item, layout);
        TextView secondaryLabelView = populateSecondaryLabelView(item, layout);
        labelView.setSingleLine(!item.isMultilineLabel());
        if (item.isMultilineLabel()) {
            // If there is a multiline label, we add extra padding at the top and bottom because
            // WRAP_CONTENT, defined above for multiline labels, leaves none.
            int existingStart = ViewCompat.getPaddingStart(labelView);
            int existingEnd = ViewCompat.getPaddingEnd(labelView);
            ViewCompat.setPaddingRelative(
                    labelView, existingStart, mLabelMargin, existingEnd, mLabelMargin);
        }

        if (item.isGroupHeader() || item.isBoldLabel()) {
            labelView.setTypeface(null, Typeface.BOLD);
            if (secondaryLabelView != null) {
                secondaryLabelView.setTypeface(null, Typeface.BOLD);
            }
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
            if (secondaryLabelView != null) {
                secondaryLabelView.setTypeface(null, Typeface.NORMAL);
            }
        }

        labelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
        labelView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                mContext.getResources().getDimension(R.dimen.text_size_large));

        if (secondaryLabelView != null) {
            secondaryLabelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
            secondaryLabelView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(R.dimen.text_size_large));
        }

        // Layout of the sublabel view, which has a smaller font and usually sits below the main
        // label.
        TextView sublabelView = populateSublabelView(item, layout);
        if (sublabelView != null) {
            sublabelView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
        }

        ImageView iconViewStart = (ImageView) layout.findViewById(R.id.start_dropdown_icon);
        ImageView iconViewEnd = (ImageView) layout.findViewById(R.id.end_dropdown_icon);
        if (item.isIconAtStart()) {
            iconViewEnd.setVisibility(View.GONE);
            iconViewStart.setVisibility(View.VISIBLE);
        } else {
            iconViewStart.setVisibility(View.GONE);
            iconViewEnd.setVisibility(View.VISIBLE);
        }

        ImageView iconView =
                populateIconView(item.isIconAtStart() ? iconViewStart : iconViewEnd, item);
        if (iconView != null) {
            iconView.setLayoutParams(getSizeAndMarginParamsForIconView(iconView, item));
        }

        return layout;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        DropdownItem item = getItem(position);
        return item.isEnabled() && !item.isGroupHeader();
    }

    /**
     * Sets the text and enabled state of the view for the first part of first line of the dropdown.
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @return the View.
     */
    private TextView populateLabelView(DropdownItem item, View layout) {
        TextView labelView = (TextView) layout.findViewById(R.id.dropdown_label);
        labelView.setEnabled(item.isEnabled());
        labelView.setText(item.getLabel());
        return labelView;
    }

    /**
     * Sets the text and enabled state of the view for the second part of first line of the
     * dropdown.
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @return the View if it has been set to be visible; null otherwise.
     */
    private TextView populateSecondaryLabelView(DropdownItem item, View layout) {
        TextView secondaryLabelView = (TextView) layout.findViewById(R.id.dropdown_secondary_label);
        CharSequence secondaryLabel = item.getSecondaryLabel();
        if (TextUtils.isEmpty(secondaryLabel)) {
            secondaryLabelView.setVisibility(View.GONE);
            return null;
        }
        secondaryLabelView.setEnabled(item.isEnabled());
        secondaryLabelView.setText(secondaryLabel);
        secondaryLabelView.setVisibility(View.VISIBLE);
        return secondaryLabelView;
    }

    /**
     * Sets the text of the sublabel's View if such text exists; otherwise, sets the View's
     * visibility to GONE.
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @return the View if it has been set to be visible; null otherwise.
     */
    private TextView populateSublabelView(DropdownItem item, View layout) {
        TextView sublabelView = (TextView) layout.findViewById(R.id.dropdown_sublabel);
        CharSequence sublabel = item.getSublabel();
        if (TextUtils.isEmpty(sublabel)) {
            sublabelView.setVisibility(View.GONE);
            return null;
        }
        sublabelView.setText(sublabel);
        sublabelView.setVisibility(View.VISIBLE);
        return sublabelView;
    }

    /**
     * Sets the text of the itemTag's View if such text exists; otherwise, sets the View's
     * visibility to GONE.
     * @param item the DropdownItem for this row.
     * @param layout the View in which the label can be found.
     * @return the View if it has been set to be visible; null otherwise. This view is already part
     *         of the view hierarchy, thus no need to explicitly add it.
     */
    private TextView populateItemTagView(DropdownItem item, View layout) {
        TextView itemTagView = (TextView) layout.findViewById(R.id.dropdown_item_tag);
        CharSequence itemTag = item.getItemTag();
        if (TextUtils.isEmpty(itemTag)) {
            itemTagView.setVisibility(View.GONE);
            return null;
        }
        itemTagView.setText(itemTag);
        itemTagView.setVisibility(View.VISIBLE);
        return itemTagView;
    }

    /**
     * Sets the drawable in the given ImageView to the resource identified in the item, or sets
     * iconView to visibility GONE if no icon is given.
     * @param iconView the ImageView which should be modified.
     * @param item the DropdownItem for this row.
     * @return |iconView| if it has been set to be visible; null otherwise.
     */
    @Nullable
    private ImageView populateIconView(ImageView iconView, DropdownItem item) {
        // If neither the iconId nor the customIcon are provided, return null as we have nothing to
        // display for the item.
        if (item.getIconId() == DropdownItem.NO_ICON && item.getCustomIcon() == null) {
            iconView.setVisibility(View.GONE);
            return null;
        }
        // If a customIcon is provided we prefer to use it over the iconId of the item.
        if (item.getCustomIcon() != null) {
            // Scale the bitmap to match the dimensions of the default resources used for other
            // items.
            Bitmap scaledBitmap = Bitmap.createScaledBitmap(item.getCustomIcon(),
                    mContext.getResources().getDimensionPixelSize(
                            R.dimen.autofill_dropdown_icon_width),
                    mContext.getResources().getDimensionPixelSize(
                            R.dimen.autofill_dropdown_icon_height),
                    true);
            iconView.setImageBitmap(scaledBitmap);
        } else {
            iconView.setImageDrawable(AppCompatResources.getDrawable(mContext, item.getIconId()));
        }
        iconView.setVisibility(View.VISIBLE);
        // TODO(crbug.com/874077): Add accessible text for this icon.
        return iconView;
    }

    /**
     * @param iconView the ImageView for which params are being generated.
     * @param item the DropdownItem for this row.
     * @return a MarginLayoutParams object with values suitable for sizing iconView.
     */
    private ViewGroup.MarginLayoutParams getSizeParamsForIconView(
            ImageView iconView, DropdownItem item) {
        ViewGroup.MarginLayoutParams iconLayoutParams =
                (ViewGroup.MarginLayoutParams) iconView.getLayoutParams();
        int iconSizeResId = item.getIconSizeResId();
        int iconSize = iconSizeResId == 0
                ? LayoutParams.WRAP_CONTENT
                : mContext.getResources().getDimensionPixelSize(iconSizeResId);
        iconLayoutParams.width = iconSize;
        iconLayoutParams.height = iconSize;
        return iconLayoutParams;
    }

    /**
     * @param iconView the ImageView for which params are being generated.
     * @param item the DropdownItem for this row.
     * @return the same as |getSizeParamsForIconView|, but with additional margin-related params
     * set.
     */
    private ViewGroup.MarginLayoutParams getSizeAndMarginParamsForIconView(
            ImageView iconView, DropdownItem item) {
        ViewGroup.MarginLayoutParams params = getSizeParamsForIconView(iconView, item);
        int iconMargin = mContext.getResources().getDimensionPixelSize(item.getIconMarginResId());
        MarginLayoutParamsCompat.setMarginStart(params, iconMargin);
        MarginLayoutParamsCompat.setMarginEnd(params, iconMargin);
        return params;
    }
}
