// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Referred from chrome/browser/media/webrtc/media_stream_capture_indicator.h

#ifndef NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_
#define NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_

#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
class WebContents;
}

class MediaStreamUI {
 public:
  virtual ~MediaStreamUI() = default;

  virtual gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) = 0;
};

namespace neva_app_runtime {

class MediaStreamCaptureIndicator
    : public base::RefCountedThreadSafe<MediaStreamCaptureIndicator> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                           bool is_capturing_video) {}
    virtual void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                           bool is_capturing_audio) {}
    virtual void OnIsBeingMirroredChanged(content::WebContents* web_contents,
                                          bool is_being_mirrored) {}
    virtual void OnIsCapturingWindowChanged(content::WebContents* web_contents,
                                            bool is_capturing_window) {}
    virtual void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                             bool is_capturing_display) {}

   protected:
    ~Observer() override;
  };

  MediaStreamCaptureIndicator();

  std::unique_ptr<content::MediaStreamUI> RegisterMediaStream(
      content::WebContents* web_contents,
      const blink::mojom::StreamDevices& devices,
      std::unique_ptr<MediaStreamUI> ui = nullptr,
      const std::u16string application_title = std::u16string());

  bool IsCapturingUserMedia(content::WebContents* web_contents) const;
  bool IsCapturingVideo(content::WebContents* web_contents) const;
  bool IsCapturingAudio(content::WebContents* web_contents) const;
  bool IsBeingMirrored(content::WebContents* web_contents) const;
  bool IsCapturingWindow(content::WebContents* web_contents) const;
  bool IsCapturingDisplay(content::WebContents* web_contents) const;
  void NotifyStopped(content::WebContents* web_contents) const;

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

 private:
  class UIDelegate;
  class WebContentsDeviceUsage;
  friend class WebContentsDeviceUsage;

  friend class base::RefCountedThreadSafe<MediaStreamCaptureIndicator>;
  virtual ~MediaStreamCaptureIndicator();

  MediaStreamCaptureIndicator(const MediaStreamCaptureIndicator&) = delete;
  MediaStreamCaptureIndicator& operator=(const MediaStreamCaptureIndicator&) =
      delete;

  void UnregisterWebContents(content::WebContents* web_contents);

  using WebContentsDeviceUsagePredicate =
      base::RepeatingCallback<bool(const WebContentsDeviceUsage*)>;
  bool CheckUsage(content::WebContents* web_contents,
                  const WebContentsDeviceUsagePredicate& pred) const;

  std::unordered_map<content::WebContents*,
                     std::unique_ptr<WebContentsDeviceUsage>>
      usage_map_;

  base::ObserverList<Observer, /* check_empty =*/true> observers_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_CAPTURE_INDICATOR_H_
