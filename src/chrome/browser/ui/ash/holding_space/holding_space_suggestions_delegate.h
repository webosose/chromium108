// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_

#include <set>

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace ash {

// A holding space delegate that manages the file suggestions (i.e. the files
// predicted to be needed by users) used in the holding space. The delegate
// observes the file suggestion service. When file suggestions update, the
// delegate refreshes the file suggestion items in the holding space model.
class HoldingSpaceSuggestionsDelegate
    : public HoldingSpaceKeyedServiceDelegate,
      public app_list::FileSuggestKeyedService::Observer {
 public:
  HoldingSpaceSuggestionsDelegate(HoldingSpaceKeyedService* service,
                                  HoldingSpaceModel* model);
  HoldingSpaceSuggestionsDelegate(const HoldingSpaceSuggestionsDelegate&) =
      delete;
  HoldingSpaceSuggestionsDelegate& operator=(
      const HoldingSpaceSuggestionsDelegate&) = delete;
  ~HoldingSpaceSuggestionsDelegate() override;

 private:
  // HoldingSpaceKeyedServiceDelegate:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override;
  void OnPersistenceRestored() override;

  // app_list::FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(app_list::FileSuggestionType type) override;

  // Fetches file suggestions of the specified `type` from the service. Returns
  // early if the fetch on the suggestions of `type` is already pending.
  void MaybeFetchSuggestions(app_list::FileSuggestionType type);

  // Called when fetching file suggestions finishes.
  void OnSuggestionsFetched(
      app_list::FileSuggestionType type,
      const absl::optional<std::vector<app_list::FileSuggestData>>&
          suggestions);

  // Updates suggestions in the holding space model. The method ensures that:
  // 1. Drive file suggestions (if any) are always in front of local file
  // suggestions; and
  // 2. The suggestions of the same type (i.e. drive file ones or local file
  // ones) follow the relevance order.
  void UpdateSuggestionsInModel();

  base::ScopedObservation<app_list::FileSuggestKeyedService,
                          app_list::FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  // Records the suggestion types on which data fetches are pending.
  std::set<app_list::FileSuggestionType> pending_fetches_;

  // Caches the most recently fetched suggestion data.
  std::map<app_list::FileSuggestionType, std::vector<app_list::FileSuggestData>>
      suggestions_by_type_;

  base::WeakPtrFactory<HoldingSpaceSuggestionsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_
