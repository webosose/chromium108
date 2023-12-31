// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"

namespace ash {

// Representation independent class that controls screen showing warning about
// malformed HWID to users.
class WrongHWIDScreen : public BaseScreen {
 public:
  WrongHWIDScreen(base::WeakPtr<WrongHWIDScreenView> view,
                  const base::RepeatingClosure& exit_callback);

  WrongHWIDScreen(const WrongHWIDScreen&) = delete;
  WrongHWIDScreen& operator=(const WrongHWIDScreen&) = delete;

  ~WrongHWIDScreen() override;

  void OnExit();

  void set_exit_callback_for_testing(
      const base::RepeatingClosure& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const base::RepeatingClosure& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<WrongHWIDScreenView> view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::WrongHWIDScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
