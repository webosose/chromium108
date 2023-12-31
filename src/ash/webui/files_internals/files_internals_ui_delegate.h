// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_
#define ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_

#include "base/values.h"

namespace ash {

// Delegate to expose //chrome services to //ash/webui FilesInternalsUI.
class FilesInternalsUIDelegate {
 public:
  virtual ~FilesInternalsUIDelegate() = default;

  virtual base::Value GetDebugJSON() const = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_UI_DELEGATE_H_
