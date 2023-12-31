// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_H_
#define ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_H_

#include "ash/webui/files_internals/files_internals_ui_delegate.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// WebUIController for chrome://files-internals/.
class FilesInternalsUI : public ui::MojoWebUIController {
 public:
  FilesInternalsUI(content::WebUI* web_ui,
                   std::unique_ptr<FilesInternalsUIDelegate> delegate);
  FilesInternalsUI(const FilesInternalsUI&) = delete;
  FilesInternalsUI& operator=(const FilesInternalsUI&) = delete;
  ~FilesInternalsUI() override;

 private:
  void HandleRequest(const std::string& url,
                     content::WebUIDataSource::GotDataCallback callback);

  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<FilesInternalsUIDelegate> delegate_;

  base::WeakPtrFactory<FilesInternalsUI> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_FILES_INTERNALS_FILES_INTERNALS_H_
