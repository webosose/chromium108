// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Fragment containing the Privacy Guide (a walk-through of the most important privacy settings).
 */
public class PrivacyGuideFragment extends Fragment {
    private BottomSheetController mBottomSheetController;
    private PrivacyGuidePagerAdapter mPagerAdapter;
    private View mView;
    private ViewPager2 mViewPager;
    private ButtonCompat mNextButton;
    private ButtonCompat mBackButton;
    private ButtonCompat mFinishButton;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        modifyAppBar();

        mView = inflater.inflate(R.layout.privacy_guide_fragment, container, false);
        displayWelcomePage();

        return mView;
    }

    private void modifyAppBar() {
        AppCompatActivity settingsActivity = (AppCompatActivity) getActivity();
        settingsActivity.setTitle(R.string.prefs_privacy_guide_title);
        settingsActivity.getSupportActionBar().setDisplayHomeAsUpEnabled(false);
    }

    private void displayWelcomePage() {
        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_welcome, content);

        ButtonCompat welcomeButton = (ButtonCompat) mView.findViewById(R.id.start_button);
        welcomeButton.setOnClickListener((View v) -> displayMainFlow());
    }

    private void displayMainFlow() {
        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_steps, content);

        mViewPager = (ViewPager2) mView.findViewById(R.id.review_viewpager);
        mPagerAdapter = new PrivacyGuidePagerAdapter(this, new StepDisplayHandlerImpl());
        mViewPager.setAdapter(mPagerAdapter);
        mViewPager.setUserInputEnabled(false);

        mNextButton = (ButtonCompat) mView.findViewById(R.id.next_button);
        mNextButton.setOnClickListener((View v) -> nextStep());

        mBackButton = (ButtonCompat) mView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener((View v) -> previousStep());

        mFinishButton = (ButtonCompat) mView.findViewById(R.id.finish_button);
        mFinishButton.setOnClickListener((View v) -> displayDonePage());
    }

    private void displayDonePage() {
        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_done, content);

        ButtonCompat doneButton = (ButtonCompat) mView.findViewById(R.id.done_button);
        doneButton.setOnClickListener((View v) -> getActivity().onBackPressed());
    }

    private void nextStep() {
        int nextIdx = mViewPager.getCurrentItem() + 1;
        if (nextIdx < mPagerAdapter.getItemCount()) {
            mViewPager.setCurrentItem(nextIdx);
        }
        mBackButton.setVisibility(View.VISIBLE);
        if (nextIdx + 1 == mPagerAdapter.getItemCount()) {
            mNextButton.setVisibility(View.GONE);
            mFinishButton.setVisibility(View.VISIBLE);
        }
    }

    private void previousStep() {
        mFinishButton.setVisibility(View.GONE);
        int prevIdx = mViewPager.getCurrentItem() - 1;
        if (prevIdx >= 0) {
            mViewPager.setCurrentItem(prevIdx);
        }
        mNextButton.setVisibility(View.VISIBLE);
        if (prevIdx == 0) {
            mBackButton.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void onAttachFragment(@NonNull Fragment childFragment) {
        if (childFragment instanceof SafeBrowsingFragment) {
            ((SafeBrowsingFragment) childFragment).setBottomSheetController(mBottomSheetController);
        }
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
        inflater.inflate(R.menu.privacy_guide_toolbar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id) {
            getActivity().onBackPressed();
            return true;
        }

        return false;
    }

    public void setBottomSheetController(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }
}
