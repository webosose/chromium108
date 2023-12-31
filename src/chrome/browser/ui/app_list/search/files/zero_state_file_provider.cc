// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/files/justifications.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

constexpr char kSchema[] = "zero_state_file://";
constexpr size_t kMaxLocalFiles = 10u;

// Screenshots are identified as files that match ScreenshotXXX.png in the
// Downloads folder.
bool IsScreenshot(const base::FilePath& path,
                  const base::FilePath& downloads_path) {
  return path.DirName() == downloads_path && path.Extension() == ".png" &&
         path.BaseName().value().rfind("Screenshot", 0) == 0;
}

// TODO(crbug.com/1258415): This exists to reroute results depending on which
// launcher is enabled, and should be removed after the new launcher launch.
ash::SearchResultDisplayType GetDisplayType() {
  return ash::features::IsProductivityLauncherEnabled()
             ? ash::SearchResultDisplayType::kContinue
             : ash::SearchResultDisplayType::kList;
}

bool IsDriveDisabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile),
      thumbnail_loader_(profile),
      file_suggest_service_(
          FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile)),
      downloads_path_(
          file_manager::util::GetDownloadsFolderForProfile(profile)) {
  DCHECK(profile_);
  file_suggest_service_observation_.Observe(file_suggest_service_);
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

ash::AppListSearchResultType ZeroStateFileProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateFile;
}

bool ZeroStateFileProvider::ShouldBlockZeroState() const {
  return true;
}

void ZeroStateFileProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
}

void ZeroStateFileProvider::StartZeroState() {
  query_start_time_ = base::TimeTicks::Now();
  ClearResultsSilently();

  // Despite this being for zero-state _local_ files only, we disable all
  // results in the Continue section if Drive is disabled.
  if (IsDriveDisabled(profile_)) {
    return;
  }

  file_suggest_service_->GetSuggestFileData(
      FileSuggestionType::kLocalFile,
      base::BindOnce(&ZeroStateFileProvider::OnSuggestFileDataFetched,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::OnSuggestFileDataFetched(
    const absl::optional<std::vector<FileSuggestData>>& suggest_results) {
  if (suggest_results)
    SetSearchResults(*suggest_results);
}

void ZeroStateFileProvider::SetSearchResults(
    const std::vector<FileSuggestData>& results) {
  // Use valid results for search results.
  SearchProvider::Results new_results;
  for (size_t i = 0; i < std::min(results.size(), kMaxLocalFiles); ++i) {
    const auto& filepath = results[i].file_path;
    if (!IsScreenshot(filepath, downloads_path_)) {
      DCHECK(results[i].score.has_value());
      auto result = std::make_unique<FileResult>(
          /*id=*/kSchema + filepath.value(), filepath,
          results[i].prediction_reason,
          ash::AppListSearchResultType::kZeroStateFile, GetDisplayType(),
          results[i].score.value(), std::u16string(), FileResult::Type::kFile,
          profile_);
      // TODO(crbug.com/1258415): Only generate thumbnails if the old launcher
      // is enabled. We should implement new thumbnail logic for Continue
      // results if necessary.
      if (result->display_type() == ash::SearchResultDisplayType::kList) {
        result->RequestThumbnail(&thumbnail_loader_);
      }
      new_results.push_back(std::move(result));
    }
  }

  if (app_list_features::IsForceShowContinueSectionEnabled())
    AppendFakeSearchResults(&new_results);

  UMA_HISTOGRAM_TIMES("Apps.AppList.ZeroStateFileProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
  SwapResults(&new_results);
}

void ZeroStateFileProvider::AppendFakeSearchResults(Results* results) {
  constexpr int kTotalFakeFiles = 3;
  for (int i = 0; i < kTotalFakeFiles; ++i) {
    const base::FilePath path(base::FilePath(FILE_PATH_LITERAL(
        base::StrCat({"Fake-file-", base::NumberToString(i), ".png"}))));
    results->emplace_back(std::make_unique<FileResult>(
        /*id=*/kSchema + path.value(), path, u"-",
        ash::AppListSearchResultType::kZeroStateFile,
        ash::SearchResultDisplayType::kContinue, 0.1f, std::u16string(),
        FileResult::Type::kFile, profile_));
  }
}

void ZeroStateFileProvider::OnFileSuggestionUpdated(FileSuggestionType type) {
  if (type == FileSuggestionType::kLocalFile)
    StartZeroState();
}

}  // namespace app_list
