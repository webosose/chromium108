// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.SingleThreadTaskRunnerImpl;
import org.chromium.base.task.TaskExecutor;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.content.browser.UiThreadTaskTraitsImpl;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

import javax.annotation.concurrent.GuardedBy;

/**
 * This {@link TaskExecutor} is for tasks posted with {@link UiThreadTaskTraits}. It maps directly
 * to content::BrowserTaskExecutor except only UI thread posting is supported from java.
 *
 * NB if you wish to post to the thread pool then use {@link TaskTraits} instead of {@link
 * UiThreadTaskTraits}.
 */
public class BrowserTaskExecutor implements TaskExecutor {
    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({BootstrapTaskRunnerType.POST_AT_BACK_OF_QUEUE,
            BootstrapTaskRunnerType.POST_AT_FRONT_OF_QUEUE, BootstrapTaskRunnerType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface BootstrapTaskRunnerType {
        int POST_AT_BACK_OF_QUEUE = 0;
        int POST_AT_FRONT_OF_QUEUE = 1;

        int NUM_ENTRIES = 2;
    }

    @Override
    public TaskRunner createTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    /**
     * This maps to a single thread within the native thread pool. Due to that contract we
     * can't run tasks posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        synchronized (mTaskRunners) {
            WeakReference<SingleThreadTaskRunner> weakRef = mTaskRunners.get(taskTraits);
            if (weakRef != null) {
                SingleThreadTaskRunner taskRunner = weakRef.get();
                if (taskRunner != null) return taskRunner;
            }

            boolean shouldPostPreNativeTasksAtFront =
                    shouldPostPreNativeTasksAtFrontOfQueue(taskTraits);
            // TODO(alexclarke): ThreadUtils.getUiThreadHandler shouldn't be in base.
            SingleThreadTaskRunner taskRunner = new SingleThreadTaskRunnerImpl(
                    ThreadUtils.getUiThreadHandler(), taskTraits, shouldPostPreNativeTasksAtFront);
            mTaskRunners.put(taskTraits, new WeakReference<>(taskRunner));

            UiThreadTaskTraitsImpl uiThreadTaskTraits =
                    taskTraits.getExtension(UiThreadTaskTraitsImpl.DESCRIPTOR);
            if (uiThreadTaskTraits != null
                    && uiThreadTaskTraits.getTaskType() == BrowserTaskType.BOOTSTRAP) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.TaskScheduling.BootstrapTaskRunnerType",
                        shouldPostPreNativeTasksAtFront
                                ? BootstrapTaskRunnerType.POST_AT_FRONT_OF_QUEUE
                                : BootstrapTaskRunnerType.POST_AT_BACK_OF_QUEUE,
                        BootstrapTaskRunnerType.NUM_ENTRIES);
            }

            return taskRunner;
        }
    }

    @Override
    public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        createSingleThreadTaskRunner(taskTraits).postDelayedTask(task, delay);
    }

    @Override
    public boolean canRunTaskImmediately(TaskTraits traits) {
        return createSingleThreadTaskRunner(traits).belongsToCurrentThread();
    }

    public static void register() {
        // In some tests we will get called multiple times.
        if (sRegistered) return;
        sRegistered = true;

        PostTask.registerTaskExecutor(
                UiThreadTaskTraitsImpl.DESCRIPTOR.getId(), new BrowserTaskExecutor());
    }

    public static boolean getShouldPrioritizePreNativeBootstrapTasks() {
        return sShouldPrioritizePreNativeBootstrapTasks;
    }

    public static void setShouldPrioritizePreNativeBootstrapTasks(
            boolean shouldPrioritizePreNativeBootstrapTasks) {
        sShouldPrioritizePreNativeBootstrapTasks = shouldPrioritizePreNativeBootstrapTasks;
    }

    private static boolean shouldPostPreNativeTasksAtFrontOfQueue(TaskTraits taskTraits) {
        if (!sShouldPrioritizePreNativeBootstrapTasks) return false;

        UiThreadTaskTraitsImpl impl = taskTraits.getExtension(UiThreadTaskTraitsImpl.DESCRIPTOR);
        if (impl == null) return false;

        switch (impl.getTaskType()) {
            case BrowserTaskType.BOOTSTRAP:
                return true;

            default:
                return false;
        }
    }

    @GuardedBy("mTaskRunners")
    private final WeakHashMap<TaskTraits, WeakReference<SingleThreadTaskRunner>> mTaskRunners =
            new WeakHashMap<>();

    private static boolean sRegistered;
    private static boolean sShouldPrioritizePreNativeBootstrapTasks;
}
