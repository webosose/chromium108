// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SHOW_SUBMIT_SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import static java.util.Arrays.asList;

import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * View tests for the Touch To Fill component ensure that model changes are reflected in the sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillViewTest {
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "", false, false, 0);
    private static final Credential NO_ONE =
            new Credential("", "***", "No Username", "m.example.xyz", true, false, 0);
    private static final Credential BOB =
            new Credential("Bob", "***", "Bob", "mobile.example.xyz", true, false, 0);
    private static final Credential NIK =
            new Credential("Nik", "***", "Nik", "group.xyz", false, true, 0);

    @Mock
    private Callback<Integer> mDismissHandler;
    @Mock
    private Callback<Credential> mCredentialCallback;

    private PropertyModel mModel;
    private TouchToFillView mTouchToFillView;
    private BottomSheetController mBottomSheetController;
    TouchToFillResourceProvider mResourceProvider;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        mResourceProvider = new TouchToFillResourceProviderImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = TouchToFillProperties.createDefaultModel(mDismissHandler);
            mTouchToFillView = new TouchToFillView(getActivity(), mBottomSheetController);
            TouchToFillCoordinator.setUpModelChangeProcessors(mModel, mTouchToFillView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mTouchToFillView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mTouchToFillView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TOUCH_TO_FILL_PASSWORD_SUBMISSION,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
    public void
    testSingleCredentialTitleDisplayedWithSubmissionDisabled() {
        // TODO(crbug.com/1283004): Remove the test once TOUCH_TO_FILL_PASSWORD_SUBMISSION is fully
        // launched.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(TITLE,
                                            getActivity().getString(
                                                    R.string.touch_to_fill_sheet_title_single))
                                    .with(FORMATTED_URL, "www.example.org")
                                    .with(ORIGIN_SECURE, true)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_title);

        assertThat(title.getText(),
                is(getActivity().getString(R.string.touch_to_fill_sheet_title_single)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
    public void testTitlePropagatesToView() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(TITLE,
                                            getActivity().getString(
                                                    R.string.touch_to_fill_sheet_uniform_title))
                                    .with(FORMATTED_URL, "www.example.org")
                                    .with(ORIGIN_SECURE, true)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_title);

        assertThat(title.getText(),
                is(getActivity().getString(R.string.touch_to_fill_sheet_uniform_title)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
    public void testManageButtonTextPropagatesToView() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(TITLE,
                                            getActivity().getString(
                                                    R.string.touch_to_fill_sheet_uniform_title))
                                    .with(FORMATTED_URL, "www.example.org")
                                    .with(ORIGIN_SECURE, true)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
            mModel.set(MANAGE_BUTTON_TEXT,
                    getActivity().getString(R.string.manage_passwords_and_passkeys));
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView manageButtonText = mTouchToFillView.getContentView().findViewById(
                R.id.touch_to_fill_sheet_manage_passwords);

        assertThat(manageButtonText.getText(),
                is(getActivity().getString(R.string.manage_passwords_and_passkeys)));
    }

    @Test
    @MediumTest
    public void testSecureSubtitleUrlDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(FORMATTED_URL, "www.example.org")
                                    .with(ORIGIN_SECURE, true)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText(), is("www.example.org"));
    }

    @Test
    @MediumTest
    public void testNonSecureSubtitleUrlDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(FORMATTED_URL, "m.example.org")
                                    .with(ORIGIN_SECURE, false)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText(), is("m.example.org (not secure)"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TOUCH_TO_FILL_PASSWORD_SUBMISSION})
    public void testSubmissionSubtitleUrlDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(SHOW_SUBMIT_SUBTITLE, true)
                                    .with(FORMATTED_URL, "m.example.org")
                                    .with(ORIGIN_SECURE, true)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText(), is("You'll sign in to m.example.org"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TOUCH_TO_FILL_PASSWORD_SUBMISSION})
    public void testNonSecureSubmissionSubtitleUrlDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .add(new MVCListAdapter.ListItem(TouchToFillProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(SHOW_SUBMIT_SUBTITLE, true)
                                    .with(FORMATTED_URL, "m.example.org")
                                    .with(ORIGIN_SECURE, false)
                                    .with(IMAGE_DRAWABLE_ID,
                                            mResourceProvider.getHeaderImageDrawableId())
                                    .build()));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TextView subtitle =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_sheet_subtitle);

        assertThat(subtitle.getText(), is("You'll sign in to m.example.org (not secure)"));
    }

    @Test
    @MediumTest
    public void testCredentialsChangedByModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTouchToFillView.setVisible(true);
            mModel.get(SHEET_ITEMS)
                    .addAll(asList(buildCredentialItem(ANA), buildCredentialItem(NO_ONE),
                            buildCredentialItem(BOB), buildCredentialItem(NIK)));
        });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(getCredentials().getChildCount(), is(4));
        assertThat(getCredentialOriginAt(0).getVisibility(), is(View.GONE));
        assertThat(getCredentialNameAt(0).getText(), is(ANA.getFormattedUsername()));
        assertThat(getCredentialPasswordAt(0).getText(), is(ANA.getPassword()));
        assertThat(getCredentialPasswordAt(0).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(1).getText(), is("m.example.xyz"));
        assertThat(getCredentialNameAt(1).getText(), is(NO_ONE.getFormattedUsername()));
        assertThat(getCredentialPasswordAt(1).getText(), is(NO_ONE.getPassword()));
        assertThat(getCredentialPasswordAt(1).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(2).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(2).getText(), is("mobile.example.xyz"));
        assertThat(getCredentialNameAt(2).getText(), is(BOB.getFormattedUsername()));
        assertThat(getCredentialPasswordAt(2).getText(), is(BOB.getPassword()));
        assertThat(getCredentialPasswordAt(2).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialOriginAt(3).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialOriginAt(3).getText(), is("group.xyz"));
        assertThat(getCredentialNameAt(3).getText(), is(NIK.getFormattedUsername()));
        assertThat(getCredentialPasswordAt(3).getText(), is(NIK.getPassword()));
        assertThat(getCredentialPasswordAt(3).getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
    }

    @Test
    @MediumTest
    public void testCredentialsAreClickable() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS).addAll(Collections.singletonList(buildCredentialItem(ANA)));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));

        TouchCommon.singleClickView(getCredentials().getChildAt(0));

        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testSingleCredentialHasClickableButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .addAll(asList(buildCredentialItem(ANA), buildConfirmationButton(ANA, false)));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertNotNull(getCredentials().getChildAt(0));
        assertNotNull(getCredentials().getChildAt(1));

        TouchCommon.singleClickView(getCredentials().getChildAt(1));

        waitForEvent(mCredentialCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testButtonTitleWithoutAutoSubmission() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .addAll(asList(buildCredentialItem(ANA), buildConfirmationButton(ANA, false)));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_button_title);

        assertThat(title.getText(), is(getActivity().getString(R.string.touch_to_fill_continue)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TOUCH_TO_FILL_PASSWORD_SUBMISSION})
    public void testButtonTitleWithAutoSubmission() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(SHEET_ITEMS)
                    .addAll(asList(buildCredentialItem(ANA), buildConfirmationButton(ANA, true)));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView title =
                mTouchToFillView.getContentView().findViewById(R.id.touch_to_fill_button_title);

        assertThat(title.getText(), is(getActivity().getString(R.string.touch_to_fill_signin)));
    }

    @Test
    @MediumTest
    public void testManagePasswordsIsClickable() {
        final AtomicBoolean manageButtonClicked = new AtomicBoolean(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ON_CLICK_MANAGE, () -> manageButtonClicked.set(true));
            mModel.set(VISIBLE, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(
                getActivity().getRootUiCoordinatorForTesting().getBottomSheetController());

        // Swipe the sheet up to it's full state.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sheetSupport.setSheetState(SheetState.FULL, false); });

        TextView manageButton = mTouchToFillView.getContentView().findViewById(
                R.id.touch_to_fill_sheet_manage_passwords);
        TouchCommon.singleClickView(manageButton);

        pollUiThread(manageButtonClicked::get);
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        verify(mDismissHandler).onResult(BottomSheetController.StateChangeReason.NONE);
    }

    private ChromeActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private RecyclerView getCredentials() {
        return mTouchToFillView.getContentView().findViewById(R.id.sheet_item_list);
    }

    private TextView getCredentialNameAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.username);
    }

    private TextView getCredentialPasswordAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.password);
    }

    private TextView getCredentialOriginAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.credential_origin);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private MVCListAdapter.ListItem buildCredentialItem(Credential credential) {
        return buildSheetItem(
                TouchToFillProperties.ItemType.CREDENTIAL, credential, mCredentialCallback, false);
    }

    private MVCListAdapter.ListItem buildConfirmationButton(
            Credential credential, boolean showSubmitButton) {
        return buildSheetItem(TouchToFillProperties.ItemType.FILL_BUTTON, credential,
                mCredentialCallback, showSubmitButton);
    }

    private static MVCListAdapter.ListItem buildSheetItem(
            @TouchToFillProperties.ItemType int itemType, Credential credential,
            Callback<Credential> callback, boolean showSubmitButton) {
        return new MVCListAdapter.ListItem(itemType,
                new PropertyModel.Builder(TouchToFillProperties.CredentialProperties.ALL_KEYS)
                        .with(CREDENTIAL, credential)
                        .with(ON_CLICK_LISTENER, callback)
                        .with(FORMATTED_ORIGIN, credential.getOriginUrl())
                        .with(SHOW_SUBMIT_BUTTON, showSubmitButton)
                        .build());
    }
}
