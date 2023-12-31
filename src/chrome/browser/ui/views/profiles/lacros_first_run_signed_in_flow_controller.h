// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class LacrosFirstRunSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  // `flow_completed_callback` gets called when the user completes the FRE.
  // It gets `maybe_callback` as a parameter which is empty in most cases but
  // must be called on a newly opened browser window if non-empty.
  LacrosFirstRunSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      base::OnceCallback<void(ProfilePicker::BrowserOpenedCallback
                                  maybe_callback)> flow_completed_callback);
  ~LacrosFirstRunSignedInFlowController() override;

  bool sync_confirmation_seen() const { return sync_confirmation_seen_; }

  base::WeakPtr<LacrosFirstRunSignedInFlowController> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // ProfilePickerSignedInFlowController:
  void Init() override;
  void FinishAndOpenBrowser(
      ProfilePicker::BrowserOpenedCallback callback) override;
  void SwitchToSyncConfirmation() override;

 protected:
  void PreShowScreenForDebug() override;

 private:
  // Tracks whether the user got to the last step of the FRE flow.
  bool sync_confirmation_seen_ = false;

  // Callback that gets called if the first run flow is completed.
  base::OnceCallback<void(ProfilePicker::BrowserOpenedCallback maybe_callback)>
      flow_completed_callback_;

  std::unique_ptr<signin::IdentityManager::Observer> can_retry_init_observer_;

  base::WeakPtrFactory<LacrosFirstRunSignedInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_
