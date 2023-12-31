// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"
#include "base/time/time.h"

namespace app_list_features {

// Please keep these features sorted.
// TODO(newcomer|weidongg): Sort these features.

// Enable app ranking models.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableAppRanker);

// Enable a model that ranks zero-state apps search result.
// TODO(crbug.com/989350): This flag can be removed once the
// AppSearchResultRanker is removed. Same with the
// AppSearchResultRankerPredictorName.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableZeroStateAppsRanker);

// Enable a model that ranks zero-state files and recent queries.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableZeroStateMixedTypesRanker);

// Enables the feature to include a single reinstallation candidate in
// zero-state.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableAppReinstallZeroState);

// Enables hashed recording of a app list launches.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableAppListLaunchRecording);

// Enables using exact string search for non latin locales.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableExactMatchForNonLatinLocale);

// Enables categorical search in the launcher.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kCategoricalSearch);

// Forces the launcher to show the continue section even if there are no file
// suggestions.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kForceShowContinueSection);

// Enables iconified text and inline icons in launcher search.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kSearchResultInlineIcon);

// Enables a fling gesture or mouse scroll from the shelf to show the bubble
// launcher.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kQuickActionShowBubbleLauncher);

// Enable shortened search result update animations when in progress animations
// are interrupted by search model updates.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kDynamicSearchUpdateAnimation);

// Controls the bubble launcher (productivity launcher in clamshell) width. When
// enabled, the bubble UI will be narrower.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kCompactBubbleLauncher);

// Enables Play Store search in the launcher.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kLauncherPlayStoreSearch);

ASH_PUBLIC_EXPORT bool IsAppRankerEnabled();
ASH_PUBLIC_EXPORT bool IsZeroStateAppsRankerEnabled();
ASH_PUBLIC_EXPORT bool IsZeroStateMixedTypesRankerEnabled();
ASH_PUBLIC_EXPORT bool IsAppReinstallZeroStateEnabled();
ASH_PUBLIC_EXPORT bool IsAppListLaunchRecordingEnabled();
ASH_PUBLIC_EXPORT bool IsExactMatchForNonLatinLocaleEnabled();
ASH_PUBLIC_EXPORT bool IsForceShowContinueSectionEnabled();
ASH_PUBLIC_EXPORT bool IsAggregatedMlSearchRankingEnabled();
ASH_PUBLIC_EXPORT bool IsLauncherSearchNormalizationEnabled();
ASH_PUBLIC_EXPORT bool IsCategoricalSearchEnabled();
ASH_PUBLIC_EXPORT bool IsSearchResultInlineIconEnabled();
ASH_PUBLIC_EXPORT bool IsQuickActionShowBubbleLauncherEnabled();
ASH_PUBLIC_EXPORT bool IsDynamicSearchUpdateAnimationEnabled();
ASH_PUBLIC_EXPORT base::TimeDelta DynamicSearchUpdateAnimationDuration();
ASH_PUBLIC_EXPORT bool IsCompactBubbleLauncherEnabled();
ASH_PUBLIC_EXPORT bool IsLauncherPlayStoreSearchEnabled();

ASH_PUBLIC_EXPORT std::string AppSearchResultRankerPredictorName();
ASH_PUBLIC_EXPORT std::string CategoricalSearchType();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
