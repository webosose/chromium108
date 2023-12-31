// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedDeque;

/**
 * Android wrapper of AccountTrackerService which provides access from the java layer.
 * It offers the capability of fetching and seeding system accounts into AccountTrackerService in
 * C++ layer, and notifies observers when it is complete.
 *
 * TODO(crbug/1176136): Move this class to components/signin/internal
 */
public class AccountTrackerService {
    /**
     * Observers the account seeding.
     */
    public interface Observer {
        /**
         * This method is invoked every time the accounts on device are seeded.
         * @param accountInfos List of all the accounts on device.
         * @param accountsChanged Whether this seeding is triggered by an accounts changed event.
         *
         * The seeding can be triggered when Chrome starts, user signs in/signs out and
         * when accounts change. Only the last scenario should trigger this call with
         * {@param accountsChanged} equal true.
         */
        void onAccountsSeeded(List<CoreAccountInfo> accountInfos, boolean accountsChanged);
    }

    private static final String TAG = "AccountService";

    @IntDef({
            AccountsSeedingStatus.NOT_STARTED,
            AccountsSeedingStatus.IN_PROGRESS,
            AccountsSeedingStatus.DONE,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface AccountsSeedingStatus {
        int NOT_STARTED = 0;
        int IN_PROGRESS = 1;
        int DONE = 2;
    }

    private final long mNativeAccountTrackerService;
    private final Queue<Runnable> mRunnablesWaitingForAccountsSeeding;
    private @AccountsSeedingStatus int mAccountsSeedingStatus;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private AccountsChangeObserver mAccountsChangeObserver;
    private boolean mExistsPendingSeedAccountsTask;

    @VisibleForTesting
    @CalledByNative
    AccountTrackerService(long nativeAccountTrackerService) {
        mNativeAccountTrackerService = nativeAccountTrackerService;
        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
        mRunnablesWaitingForAccountsSeeding = new ConcurrentLinkedDeque<>();
        mExistsPendingSeedAccountsTask = false;
    }

    /**
     * Adds an observer to observe the accounts seeding changes.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to observe the accounts seeding changes.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Seeds the accounts only if they are not seeded yet.
     * The given runnable will run after the accounts are seeded. If the accounts
     * are already seeded, the runnable will be executed immediately.
     */
    public void seedAccountsIfNeeded(Runnable onAccountsSeeded) {
        ThreadUtils.assertOnUiThread();
        switch (mAccountsSeedingStatus) {
            case AccountsSeedingStatus.NOT_STARTED:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                seedAccounts(/*accountsChanged=*/false);
                break;
            case AccountsSeedingStatus.IN_PROGRESS:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                break;
            case AccountsSeedingStatus.DONE:
                onAccountsSeeded.run();
                break;
        }
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     * This is invoked when accounts change on device. When there is already a seeding in
     * progress, the {@link #seedAccounts()} task will be added to the pending task list so
     * that we will seed the accounts again in the end of the current seeding to avoid
     * the race condition.
     */
    public void onAccountsChanged() {
        if (mAccountsSeedingStatus == AccountsSeedingStatus.IN_PROGRESS) {
            mExistsPendingSeedAccountsTask = true;
        } else {
            seedAccounts(/*accountsChanged=*/true);
        }
    }

    /**
     * Seeds the accounts on device.
     * @param accountsChanged Whether this seeding is triggered by an accounts changed event.
     *
     * The seeding can be triggered when Chrome starts, user signs in/signs out and
     * when accounts change. Only the last scenario should trigger this call with
     * {@param accountsChanged} equal true.
     * When Chrome starts, we should trigger seedAccounts(false) because the accounts are
     * already loaded in PO2TS in this flow. accountsChanged=false will avoid it to get
     * triggered again from SigninChecker.
     */
    private void seedAccounts(boolean accountsChanged) {
        ThreadUtils.assertOnUiThread();
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        assert mAccountsSeedingStatus
                != AccountsSeedingStatus.IN_PROGRESS : "There is already a seeding in progress!";
        mAccountsSeedingStatus = AccountsSeedingStatus.IN_PROGRESS;

        if (mAccountsChangeObserver == null) {
            mAccountsChangeObserver = this::onAccountsChanged;
            accountManagerFacade.addObserver(mAccountsChangeObserver);
        }

        accountManagerFacade.getAccounts().then(accounts -> {
            final List<String> emails = AccountUtils.toAccountNames(accounts);
            new AsyncTask<List<String>>() {
                @Override
                public List<String> doInBackground() {
                    Log.d(TAG, "Getting id/email mapping");
                    final long seedingStartTime = SystemClock.elapsedRealtime();
                    final List<String> gaiaIds = new ArrayList<>();
                    for (String email : emails) {
                        final String gaiaId = accountManagerFacade.getAccountGaiaId(email);
                        if (gaiaId == null) {
                            return gaiaIds;
                        }
                        gaiaIds.add(gaiaId);
                    }
                    RecordHistogram.recordTimesHistogram("Signin.AndroidGetAccountIdsTime",
                            SystemClock.elapsedRealtime() - seedingStartTime);
                    return gaiaIds;
                }
                @Override
                public void onPostExecute(List<String> gaiaIds) {
                    if (gaiaIds.size() == emails.size()) {
                        finishSeedingAccounts(gaiaIds, emails, accountsChanged);
                    } else {
                        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
                        seedAccounts(/*accountsChanged=*/accountsChanged);
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        });
    }

    private void finishSeedingAccounts(
            List<String> gaiaIds, List<String> emails, boolean accountsChanged) {
        assert gaiaIds.size() == emails.size() : "gaia IDs and emails should have the same size!";
        AccountTrackerServiceJni.get().seedAccountsInfo(mNativeAccountTrackerService,
                gaiaIds.toArray(new String[0]), emails.toArray(new String[0]));
        mAccountsSeedingStatus = AccountsSeedingStatus.DONE;

        if (mExistsPendingSeedAccountsTask) {
            // When mExistsPendingSeedAccountsTask is true, it means that an accounts changed
            // event has been triggered during the current seeding, we should stop the current
            // seeding here and re-seed the accounts
            seedAccounts(/*accountsChanged=*/true);
            mExistsPendingSeedAccountsTask = false;
            return;
        }

        for (@Nullable Runnable runnable = mRunnablesWaitingForAccountsSeeding.poll();
                runnable != null; runnable = mRunnablesWaitingForAccountsSeeding.poll()) {
            runnable.run();
        }

        final List<CoreAccountInfo> accountInfos = new ArrayList<>();
        for (int i = 0; i < gaiaIds.size(); ++i) {
            accountInfos.add(
                    CoreAccountInfo.createFromEmailAndGaiaId(emails.get(i), gaiaIds.get(i)));
        }
        for (Observer observer : mObservers) {
            observer.onAccountsSeeded(accountInfos, accountsChanged);
        }
    }

    @NativeMethods
    interface Natives {
        void seedAccountsInfo(long nativeAccountTrackerService, String[] gaiaIds, String[] emails);
    }
}
