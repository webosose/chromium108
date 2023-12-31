// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
#define CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace chrome {
namespace android {

// Alphabetical:
BASE_DECLARE_FEATURE(kAdaptiveButtonInTopToolbar);
BASE_DECLARE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2);
BASE_DECLARE_FEATURE(kAddToHomescreenIPH);
BASE_DECLARE_FEATURE(kAllowNewIncognitoTabIntents);
BASE_DECLARE_FEATURE(kAndroidScrollOptimizations);
BASE_DECLARE_FEATURE(kAndroidSearchEngineChoiceNotification);
BASE_DECLARE_FEATURE(kAssistantConsentModal);
BASE_DECLARE_FEATURE(kAssistantConsentSimplifiedText);
BASE_DECLARE_FEATURE(kAssistantConsentV2);
BASE_DECLARE_FEATURE(kAssistantIntentExperimentId);
BASE_DECLARE_FEATURE(kAssistantIntentPageUrl);
BASE_DECLARE_FEATURE(kAssistantIntentTranslateInfo);
BASE_DECLARE_FEATURE(kAssistantNonPersonalizedVoiceSearch);
BASE_DECLARE_FEATURE(kAppLaunchpad);
BASE_DECLARE_FEATURE(kAppMenuMobileSiteOption);
BASE_DECLARE_FEATURE(kAppToWebAttribution);
BASE_DECLARE_FEATURE(kBackgroundThreadPool);
BASE_DECLARE_FEATURE(kClearOmniboxFocusAfterNavigation);
BASE_DECLARE_FEATURE(kCloseTabSuggestions);
BASE_DECLARE_FEATURE(kCriticalPersistedTabData);
BASE_DECLARE_FEATURE(kCommerceCoupons);
BASE_DECLARE_FEATURE(kCastDeviceFilter);
BASE_DECLARE_FEATURE(kCCTBackgroundTab);
BASE_DECLARE_FEATURE(kCCTBrandTransparency);
BASE_DECLARE_FEATURE(kCCTClientDataHeader);
BASE_DECLARE_FEATURE(kCCTIncognito);
BASE_DECLARE_FEATURE(kCCTIncognitoAvailableToThirdParty);
BASE_DECLARE_FEATURE(kCCTNewDownloadTab);
BASE_DECLARE_FEATURE(kCCTPackageNameRecording);
BASE_DECLARE_FEATURE(kCCTPostMessageAPI);
BASE_DECLARE_FEATURE(kCCTRealTimeEngagementSignals);
BASE_DECLARE_FEATURE(kCCTRedirectPreconnect);
BASE_DECLARE_FEATURE(kCCTRemoveRemoteViewIds);
BASE_DECLARE_FEATURE(kCCTReportParallelRequestStatus);
BASE_DECLARE_FEATURE(kCCTResizable90MaximumHeight);
BASE_DECLARE_FEATURE(kCCTResizableAllowResizeByUserGesture);
BASE_DECLARE_FEATURE(kCCTResizableAlwaysShowNavBarButtons);
BASE_DECLARE_FEATURE(kCCTResizableForFirstParties);
BASE_DECLARE_FEATURE(kCCTResizableForThirdParties);
BASE_DECLARE_FEATURE(kCCTResizableWindowAboveNavbar);
BASE_DECLARE_FEATURE(kCCTResourcePrefetch);
BASE_DECLARE_FEATURE(kCCTRetainingState);
BASE_DECLARE_FEATURE(kCCTShowAboutBlankUrl);
BASE_DECLARE_FEATURE(kCCTToolbarCustomizations);
BASE_DECLARE_FEATURE(kDiscardOccludedBitmaps);
BASE_DECLARE_FEATURE(kDontAutoHideBrowserControls);
BASE_DECLARE_FEATURE(kCacheDeprecatedSystemLocationSetting);
BASE_DECLARE_FEATURE(kChromeNewDownloadTab);
BASE_DECLARE_FEATURE(kChromeShareLongScreenshot);
BASE_DECLARE_FEATURE(kChromeShareScreenshot);
BASE_DECLARE_FEATURE(kChromeSharingHub);
BASE_DECLARE_FEATURE(kChromeSharingHubLaunchAdjacent);
BASE_DECLARE_FEATURE(kChromeSurveyNextAndroid);
BASE_DECLARE_FEATURE(kCommandLineOnNonRooted);
BASE_DECLARE_FEATURE(kConditionalTabStripAndroid);
BASE_DECLARE_FEATURE(kContextMenuEnableLensShoppingAllowlist);
BASE_DECLARE_FEATURE(kContextMenuGoogleLensChip);
BASE_DECLARE_FEATURE(kContextMenuPopupForAllScreenSizes);
BASE_DECLARE_FEATURE(kContextMenuSearchWithGoogleLens);
BASE_DECLARE_FEATURE(kContextMenuShopWithGoogleLens);
BASE_DECLARE_FEATURE(kContextMenuSearchAndShopWithGoogleLens);
BASE_DECLARE_FEATURE(kContextMenuTranslateWithGoogleLens);
BASE_DECLARE_FEATURE(kContextualSearchDelayedIntelligence);
BASE_DECLARE_FEATURE(kContextualSearchDisableOnlineDetection);
BASE_DECLARE_FEATURE(kContextualSearchForceCaption);
BASE_DECLARE_FEATURE(kContextualSearchSuppressShortView);
BASE_DECLARE_FEATURE(kContextualSearchThinWebViewImplementation);
BASE_DECLARE_FEATURE(kContextualTriggersSelectionHandles);
BASE_DECLARE_FEATURE(kContextualTriggersSelectionMenu);
BASE_DECLARE_FEATURE(kContextualTriggersSelectionSize);
BASE_DECLARE_FEATURE(kDirectActions);
BASE_DECLARE_FEATURE(kDisableCompositedProgressBar);
BASE_DECLARE_FEATURE(kDontPrefetchLibraries);
BASE_DECLARE_FEATURE(kDownloadAutoResumptionThrottling);
BASE_DECLARE_FEATURE(kDownloadFileProvider);
BASE_DECLARE_FEATURE(kDownloadHomeForExternalApp);
BASE_DECLARE_FEATURE(kDownloadNotificationBadge);
BASE_DECLARE_FEATURE(kDownloadRename);
BASE_DECLARE_FEATURE(kDuetTabStripIntegrationAndroid);
BASE_DECLARE_FEATURE(kExperimentsForAgsa);
BASE_DECLARE_FEATURE(kExploreSites);
BASE_DECLARE_FEATURE(kFocusOmniboxInIncognitoTabIntents);
BASE_DECLARE_FEATURE(kGridTabSwitcherForTablets);
BASE_DECLARE_FEATURE(kHandleMediaIntents);
BASE_DECLARE_FEATURE(kImmersiveUiMode);
BASE_DECLARE_FEATURE(kIncognitoReauthenticationForAndroid);
BASE_DECLARE_FEATURE(kIncognitoScreenshot);
BASE_DECLARE_FEATURE(kInfobarScrollOptimization);
BASE_DECLARE_FEATURE(kImprovedA2HS);
BASE_DECLARE_FEATURE(kInstanceSwitcher);
BASE_DECLARE_FEATURE(kInstantStart);
BASE_DECLARE_FEATURE(kIsVoiceSearchEnabledCache);
BASE_DECLARE_FEATURE(kKitKatSupported);
BASE_DECLARE_FEATURE(kLanguagesPreference);
BASE_DECLARE_FEATURE(kLensCameraAssistedSearch);
BASE_DECLARE_FEATURE(kLensOnQuickActionSearchWidget);
BASE_DECLARE_FEATURE(kLocationBarModelOptimizations);
BASE_DECLARE_FEATURE(kNewInstanceFromDraggedLink);
BASE_DECLARE_FEATURE(kNewTabPageTilesTitleWrapAround);
BASE_DECLARE_FEATURE(kNewWindowAppMenu);
BASE_DECLARE_FEATURE(kNotificationPermissionVariant);
BASE_DECLARE_FEATURE(kOmahaMinSdkVersionAndroid);
BASE_DECLARE_FEATURE(kOmniboxModernizeVisualUpdate);
BASE_DECLARE_FEATURE(kOptimizeGeolocationHeaderGeneration);
BASE_DECLARE_FEATURE(kPageAnnotationsService);
BASE_DECLARE_FEATURE(kBookmarksImprovedSaveFlow);
BASE_DECLARE_FEATURE(kBookmarksRefresh);
BASE_DECLARE_FEATURE(kBackGestureRefactorAndroid);
BASE_DECLARE_FEATURE(kOptimizeLayoutsForPullRefresh);
BASE_DECLARE_FEATURE(kPostTaskFocusTab);
BASE_DECLARE_FEATURE(kProbabilisticCryptidRenderer);
BASE_DECLARE_FEATURE(kReachedCodeProfiler);
BASE_DECLARE_FEATURE(kReengagementNotification);
BASE_DECLARE_FEATURE(kReaderModeInCCT);
BASE_DECLARE_FEATURE(kRelatedSearches);
BASE_DECLARE_FEATURE(kRelatedSearchesAlternateUx);
BASE_DECLARE_FEATURE(kRelatedSearchesInBar);
BASE_DECLARE_FEATURE(kRelatedSearchesSimplifiedUx);
BASE_DECLARE_FEATURE(kRelatedSearchesUi);
BASE_DECLARE_FEATURE(kRequestDesktopSiteDefaults);
BASE_DECLARE_FEATURE(kRequestDesktopSiteDefaultsControl);
BASE_DECLARE_FEATURE(kRequestDesktopSiteDefaultsControlSynthetic);
BASE_DECLARE_FEATURE(kRequestDesktopSiteDefaultsSynthetic);
BASE_DECLARE_FEATURE(kRequestDesktopSiteOptInControlSynthetic);
BASE_DECLARE_FEATURE(kRequestDesktopSiteOptInSynthetic);
BASE_DECLARE_FEATURE(kRequestDesktopSiteDefaultsDowngrade);
BASE_DECLARE_FEATURE(kSearchEnginePromoExistingDevice);
BASE_DECLARE_FEATURE(kSearchEnginePromoExistingDeviceV2);
BASE_DECLARE_FEATURE(kSearchEnginePromoNewDevice);
BASE_DECLARE_FEATURE(kSearchEnginePromoNewDeviceV2);
BASE_DECLARE_FEATURE(kShareButtonInTopToolbar);
BASE_DECLARE_FEATURE(kSharingHubLinkToggle);
BASE_DECLARE_FEATURE(kShowScrollableMVTOnNTPAndroid);
BASE_DECLARE_FEATURE(kFeedPositionAndroid);
BASE_DECLARE_FEATURE(kSafeModeForCachedFlags);
BASE_DECLARE_FEATURE(kSearchResumptionModuleAndroid);
BASE_DECLARE_FEATURE(kSpannableInlineAutocomplete);
BASE_DECLARE_FEATURE(kSpecialLocaleWrapper);
BASE_DECLARE_FEATURE(kSpecialUserDecision);
BASE_DECLARE_FEATURE(kSplitCompositorTask);
BASE_DECLARE_FEATURE(kStoreHoursAndroid);
BASE_DECLARE_FEATURE(kSuppressToolbarCaptures);
BASE_DECLARE_FEATURE(kSwapPixelFormatToFixConvertFromTranslucent);
BASE_DECLARE_FEATURE(kTabEngagementReportingAndroid);
BASE_DECLARE_FEATURE(kTabGroupsAndroid);
BASE_DECLARE_FEATURE(kTabGroupsContinuationAndroid);
BASE_DECLARE_FEATURE(kTabGroupsUiImprovementsAndroid);
BASE_DECLARE_FEATURE(kTabGroupsForTablets);
BASE_DECLARE_FEATURE(kTabGridLayoutAndroid);
BASE_DECLARE_FEATURE(kTabReparenting);
BASE_DECLARE_FEATURE(kTabSelectionEditorV2);
BASE_DECLARE_FEATURE(kTabStripImprovements);
BASE_DECLARE_FEATURE(kDiscoverFeedMultiColumn);
BASE_DECLARE_FEATURE(kTabStripRedesign);
BASE_DECLARE_FEATURE(kTabSwitcherOnReturn);
BASE_DECLARE_FEATURE(kTabToGTSAnimation);
BASE_DECLARE_FEATURE(kTestDefaultDisabled);
BASE_DECLARE_FEATURE(kTestDefaultEnabled);
BASE_DECLARE_FEATURE(kToolbarMicIphAndroid);
BASE_DECLARE_FEATURE(kToolbarPhoneOptimizations);
BASE_DECLARE_FEATURE(kToolbarScrollAblationAndroid);
BASE_DECLARE_FEATURE(kToolbarUseHardwareBitmapDraw);
BASE_DECLARE_FEATURE(kTrustedWebActivityPostMessage);
BASE_DECLARE_FEATURE(kTrustedWebActivityQualityEnforcement);
BASE_DECLARE_FEATURE(kTrustedWebActivityQualityEnforcementForced);
BASE_DECLARE_FEATURE(kTrustedWebActivityQualityEnforcementWarning);
BASE_DECLARE_FEATURE(kShowExtendedPreloadingSetting);
BASE_DECLARE_FEATURE(kStartSurfaceAndroid);
BASE_DECLARE_FEATURE(kStartSurfaceReturnTime);
BASE_DECLARE_FEATURE(kStartSurfaceRefactor);
BASE_DECLARE_FEATURE(kStartSurfaceDisabledFeedImprovement);
BASE_DECLARE_FEATURE(kUmaBackgroundSessions);
BASE_DECLARE_FEATURE(kUpdateHistoryEntryPointsInIncognito);
BASE_DECLARE_FEATURE(kUpdateNotificationScheduleServiceImmediateShowOption);
BASE_DECLARE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid);
BASE_DECLARE_FEATURE(kUserMediaScreenCapturing);
BASE_DECLARE_FEATURE(kVoiceSearchAudioCapturePolicy);
BASE_DECLARE_FEATURE(kVoiceButtonInTopToolbar);
BASE_DECLARE_FEATURE(kVrBrowsingFeedback);
BASE_DECLARE_FEATURE(kWebOtpCrossDeviceSimpleString);
BASE_DECLARE_FEATURE(kWebApkInstallService);
BASE_DECLARE_FEATURE(kWebApkTrampolineOnInitialIntent);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_FLAGS_ANDROID_CHROME_FEATURE_LIST_H_
