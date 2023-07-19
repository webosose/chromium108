// Copyright 2022 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_BROWSER_WRAP_CALLBACK_H_
#define NEVA_APP_RUNTIME_BROWSER_WRAP_CALLBACK_H_

#include "base/callback.h"
#include "neva/app_runtime/public/callback_helper.h"

namespace neva_app_runtime {

template <typename Signature>
class WrapOnceCallback;
template <typename R, typename... Args>
class WrapOnceCallback<R(Args...)>
    : public CallbackHelper<R(Args...)>::Callback {
 public:
  using RunType = R(Args...);
  using CallbackType = base::OnceCallback<RunType>;

  WrapOnceCallback() = default;
  WrapOnceCallback(CallbackType callback) : callback_(std::move(callback)) {}
  ~WrapOnceCallback() override {}

  void operator()(Args... args) {
    if (callback_)
      std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  CallbackType callback_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_WRAP_CALLBACK_H_
