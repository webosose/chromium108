// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features.h"

namespace arc {

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
BASE_FEATURE(kBootCompletedBroadcastFeature,
             "ArcBootCompletedBroadcast",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls experimental Custom Tabs feature for ARC.
BASE_FEATURE(kCustomTabsExperimentFeature,
             "ArcCustomTabsExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to handle files with unknown size.
BASE_FEATURE(kDocumentsProviderUnknownSizeFeature,
             "ArcDocumentsProviderUnknownSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether files shared to ARC Nearby Share are shared through the
// FuseBox filesystem, instead of the default method (through a temporary path
// managed by file manager).
BASE_FEATURE(kEnableArcNearbyShareFuseBox,
             "ArcNearbyShareFuseBox",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether crosvm for ARCVM does per-VM core scheduling on devices with
// MDS/L1TF vulnerabilities. When this feature is disabled, crosvm does per-vCPU
// core scheduling which is more secure.
//
// How to safely disable this feature for security (or other) reasons:
//
// 1) Visit go/stainless and verify arc.Boot.vm_with_per_vcpu_core_scheduling is
//    green (it should always be because arc.Boot is a critical test.)
// 2) Change the default value of this feature to FEATURE_DISABLED_BY_DEFAULT.
// 3) Monitor arc.Boot.vm at go/stainless after Chrome is rolled.
// 4) Ask ARC team (//ash/components/arc/OWNERS) to update arc.CPUSet.vm test
//    so the Tast test uses the updated ArcEnablePerVmCoreScheduling setting.
BASE_FEATURE(kEnablePerVmCoreScheduling,
             "ArcEnablePerVmCoreScheduling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use ARC TTS caching to optimize ARC boot.
BASE_FEATURE(kEnableTTSCaching,
             "ArcEnableTTSCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use pregenerated ARC TTS cache to optimize ARC boot and
// also whether or not TTS cache is used.
BASE_FEATURE(kEnableTTSCacheSetup,
             "ArcEnableTTSCacheSetup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we should delegate audio focus requests from ARC to Chrome.
BASE_FEATURE(kEnableUnifiedAudioFocusFeature,
             "ArcEnableUnifiedAudioFocus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC handles unmanaged->managed account transition.
BASE_FEATURE(kEnableUnmanagedToManagedTransitionFeature,
             "ArcEnableUnmanagedToManagedTransitionFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC Unspecialized Application Processes.
// When enabled, Android creates a pool of processes
// that will start applications so that zygote doesn't have to wake.
BASE_FEATURE(kEnableUsap, "ArcEnableUsap", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use virtio-blk for Android /data instead of using
// virtio-fs.
BASE_FEATURE(kEnableVirtioBlkForData,
             "ArcEnableVirtioBlkForData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to pop up ghost window for ARC app before fixup finishes.
BASE_FEATURE(kFixupWindowFeature,
             "ArcFixupWindowFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used for overriding config params for the virtio-blk feature above.
BASE_FEATURE(kVirtioBlkDataConfigOverride,
             "ArcVirtioBlkDataConfigOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the LVM-provided disk as the backend device for
// Android /data instead of using the concierge-provided disk.
const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm{
    &kVirtioBlkDataConfigOverride, "use_lvm", false};

// Indicates whether LVM application containers feature is supported.
BASE_FEATURE(kLvmApplicationContainers,
             "ArcLvmApplicationContainers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental file picker feature for ARC.
BASE_FEATURE(kFilePickerExperimentFeature,
             "ArcFilePickerExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARCVM can request resourced make more resources available
// for a currently-active ARCVM game.
BASE_FEATURE(kGameModeFeature,
             "ArcGameModeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the guest zram is enabled. This is only for ARCVM.
BASE_FEATURE(kGuestZram, "ArcGuestZram", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the size of the guest zram.
const base::FeatureParam<int> kGuestZramSize{&kGuestZram, "size", 0};

// Controls swappiness for the ARCVM guest.
const base::FeatureParam<int> kGuestZramSwappiness{&kGuestZram, "swappiness",
                                                   0};

// Enables/disables mlock() of guest memory for ARCVM.
// Often used in combination with kGuestZram.
BASE_FEATURE(kLockGuestMemory,
             "ArcLockGuestMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Control properties of Logd at boot time. This is only for ARCVM.
BASE_FEATURE(kLogdConfig,
             "ArcGuestLogdConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the size in KB of logd. Only a few sizes are supported,
// see kLogdConfigSize* private constants in arc_vm_client_adapter.cc.
// The default set here means "do not override the build setting",
// which is the same behavior as disabling the feature. Doing it so,
// we don't need to keep this code up-to-date with the build default.
const base::FeatureParam<int> kLogdConfigSize{&kLogdConfig, "size", 0};

// Controls keyboard shortcut helper integration feature in ARC.
BASE_FEATURE(kKeyboardShortcutHelperIntegrationFeature,
             "ArcKeyboardShortcutHelperIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
BASE_FEATURE(kNativeBridgeToggleFeature,
             "ArcNativeBridgeExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, utility processes are spawned to perform hardware decode
// acceleration on behalf of ARC++/ARCVM instead of using the GPU process.
BASE_FEATURE(kOutOfProcessVideoDecoding,
             "OutOfProcessVideoDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARC picture-in-picture feature. If this is enabled, then Android
// will control which apps can enter PIP. If this is disabled, then ARC PIP
// will be disabled.
BASE_FEATURE(kPictureInPictureFeature,
             "ArcPictureInPicture",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 2 logical cores
// online.
// When you change the default, you also need to change the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// match it to the new default.
BASE_FEATURE(kRtVcpuDualCore,
             "ArcRtVcpuDualCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 3+ logical cores
// online.
// When you change the default, you also need to modify the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// add ArcRtVcpuQuadCore there. Otherwise, the test will start failing.
BASE_FEATURE(kRtVcpuQuadCore,
             "ArcRtVcpuQuadCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, tracing raw files are saved in order to help debug failures.
BASE_FEATURE(kSaveRawFilesOnTracing,
             "ArcSaveRawFilesOnTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, unclaimed USB device will be attached to ARCVM by default.
BASE_FEATURE(kUsbDeviceDefaultAttachToArcVm,
             "UsbDeviceDefaultAttachToArcVm",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC USB Storage UI feature.
// When enabled, chrome://settings and Files.app will ask if the user wants
// to expose USB storage devices to ARC.
BASE_FEATURE(kUsbStorageUIFeature,
             "ArcUsbStorageUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC dalvik memory profile in ARCVM.
// When enabled, Android tries to use dalvik memory profile tuned based on the
// device memory size.
BASE_FEATURE(kUseDalvikMemoryProfile,
             "ArcUseDalvikMemoryProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the system/vendor images are mounted without specifying a
// block size.
BASE_FEATURE(kUseDefaultBlockSize,
             "ArcVmUseDefaultBlockSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
BASE_FEATURE(kVideoDecoder,
             "ArcVideoDecoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to continuously log PSI memory pressure data to Chrome.
BASE_FEATURE(kVmMemoryPSIReports,
             "ArcVmMemoryPSIReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls how frequently memory pressure data is logged
const base::FeatureParam<int> kVmMemoryPSIReportsPeriod{&kVmMemoryPSIReports,
                                                        "period", 10};

// Controls whether a custom memory size is used when creating ARCVM. When
// enabled, ARCVM is sized with the following formula:
//  min(max_mib, RAM + shift_mib)
// If disabled, memory is sized by concierge which, at the time of writing, uses
// RAM - 1024 MiB.
BASE_FEATURE(kVmMemorySize,
             "ArcVmMemorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the amount to "shift" system RAM when sizing ARCVM. The default
// value of 0 means that ARCVM's memory will be thr same as the system.
const base::FeatureParam<int> kVmMemorySizeShiftMiB{&kVmMemorySize, "shift_mib",
                                                    0};

// Controls the maximum amount of memory to give ARCVM. The default value of
// INT32_MAX means that ARCVM's memory is not capped.
const base::FeatureParam<int> kVmMemorySizeMaxMiB{&kVmMemorySize, "max_mib",
                                                  INT32_MAX};

// Controls whether to use the new limit cache balloon policy. If disabled the
// old balance available balloon policy is used. If enabled, ChromeOS's Resource
// Manager (resourced) is able to kill ARCVM apps by sending a memory pressure
// signal.
// The limit cache balloon policy inflates the balloon to limit the kernel page
// cache inside ARCVM if memory in the host is low. See FeatureParams below for
// the conditions that limit cache. See mOomMinFreeHigh and mOomAdj in
// frameworks/base/services/core/java/com/android/server/am/ProcessList.java
// to see how LMKD maps kernel page cache to a priority level of app to kill.
// To ensure fairness between tab manager discards and ARCVM low memory kills,
// we want to stop LMKD killing things out of turn. We do this by making sure
// ARCVM never has it's kernel page cache drop below the level that LMKD will
// start killing.
BASE_FEATURE(kVmBalloonPolicy,
             "ArcVmBalloonPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is under
// moderate memory pressure. 0 for no limit.
const base::FeatureParam<int> kVmBalloonPolicyModerateKiB{&kVmBalloonPolicy,
                                                          "moderate_kib", 0};

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is under
// critical memory pressure. 0 for no limit. The default value of 322560KiB
// corresponds to the level LMKD will start to kill the lowest priority cached
// app.
const base::FeatureParam<int> kVmBalloonPolicyCriticalKiB{
    &kVmBalloonPolicy, "critical_kib", 322560};

// The maximum amount of kernel page cache ARCVM can have when ChromeOS is
// reclaiming. 0 for no limit. The default value of 322560KiB corresponds to the
// level LMKD will start to kill the lowest priority cached app.
const base::FeatureParam<int> kVmBalloonPolicyReclaimKiB{&kVmBalloonPolicy,
                                                         "reclaim_kib", 322560};

// Controls experimental key GMS Core and related services protection against to
// be killed by low memory killer in ARCVM.
BASE_FEATURE(kVmGmsCoreLowMemoryKillerProtection,
             "ArcVmGmsCoreLowMemoryKillerProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls experimental key to enable pre-ANR handling for BroadcastQueue in
// ARCVM.
BASE_FEATURE(kVmBroadcastPreNotifyANR,
             "ArcVmBroadcastPreAnrHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If set, enable responsive balloon sizing. Concierge will listen on a VSOCK
// for connections from LMKD in Android. When LMKD is about to kill an App, it
// will signal the balloon sizing code, which may deflate the balloon instead
// of killing the app.
const base::FeatureParam<bool> kVmBalloonPolicyResponsive{&kVmBalloonPolicy,
                                                          "responsive", true};

// The amount of time LMKD will wait for a response from concierge before
// killing an app.
const base::FeatureParam<int> kVmBalloonPolicyResponsiveTimeoutMs{
    &kVmBalloonPolicy, "responsive_timeout_ms", 100};

// If an app should not be killed, the balloon will be deflated by
// min(app_size, responsive_max_deflate_bytes), so that large apps don't
// completely deflate the balloon.
const base::FeatureParam<int> kVmBalloonPolicyResponsiveMaxDeflateBytes{
    &kVmBalloonPolicy, "responsive_max_deflate_bytes", 256 * 1024 * 1024};

}  // namespace arc
