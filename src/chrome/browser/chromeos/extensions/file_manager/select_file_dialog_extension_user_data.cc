// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/select_file_dialog_extension_user_data.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

const char kSelectFileDialogExtensionUserDataKey[] =
    "SelectFileDialogExtensionUserDataKey";

SelectFileDialogExtensionUserData::~SelectFileDialogExtensionUserData() =
    default;

// static
void SelectFileDialogExtensionUserData::SetDialogDataForWebContents(
    content::WebContents* web_contents,
    const std::string& routing_id,
    absl::optional<policy::DlpFilesController::DlpFileDestination>
        dialog_caller) {
  DCHECK(web_contents);
  web_contents->SetUserData(
      kSelectFileDialogExtensionUserDataKey,
      base::WrapUnique(new SelectFileDialogExtensionUserData(
          routing_id, std::move(dialog_caller))));
}

// static
std::string SelectFileDialogExtensionUserData::GetRoutingIdForWebContents(
    content::WebContents* web_contents) {
  // There's a race condition. This can be called from a callback after the
  // webcontents has been deleted.
  if (!web_contents) {
    LOG(WARNING) << "WebContents already destroyed.";
    return "";
  }

  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->routing_id() : "";
}

// static
absl::optional<policy::DlpFilesController::DlpFileDestination>
SelectFileDialogExtensionUserData::GetDialogCallerForWebContents(
    content::WebContents* web_contents) {
  // There's a race condition. This can be called from a callback after the
  // webcontents has been deleted.
  if (!web_contents) {
    LOG(WARNING) << "WebContents already destroyed.";
    return absl::nullopt;
  }

  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->dialog_caller() : absl::nullopt;
}

SelectFileDialogExtensionUserData::SelectFileDialogExtensionUserData(
    const std::string& routing_id,
    absl::optional<policy::DlpFilesController::DlpFileDestination>
        dialog_caller)
    : routing_id_(routing_id), dialog_caller_(std::move(dialog_caller)) {}
