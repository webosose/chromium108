// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

// Provides a mojo endpoint in the browser that allows the renderer process to
// launch and initialize the sandboxed speech recognition service
// process.
class ChromeSpeechRecognitionService : public SpeechRecognitionService {
 public:
  explicit ChromeSpeechRecognitionService(content::BrowserContext* context);
  ChromeSpeechRecognitionService(const ChromeSpeechRecognitionService&) =
      delete;
  ChromeSpeechRecognitionService& operator=(const SpeechRecognitionService&) =
      delete;
  ~ChromeSpeechRecognitionService() override;

  // SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) override;

 private:
  // Launches the speech recognition service in a sandboxed utility process.
  void LaunchIfNotRunning();

  // Gets the path of the SODA configuration file for the selected language.
  base::FilePath GetSodaConfigPath(PrefService* prefs);

  // The browser context associated with the keyed service.
  const raw_ptr<content::BrowserContext> context_;

  // The remote to the speech recognition service. The browser will not launch a
  // new speech recognition service process if this remote is already bound.
  mojo::Remote<media::mojom::SpeechRecognitionService>
      speech_recognition_service_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_
