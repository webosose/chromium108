// Copyright 2022 LG Electronics, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_PUBLIC_CALLBACK_HELPER_H_
#define NEVA_APP_RUNTIME_PUBLIC_CALLBACK_HELPER_H_

namespace neva_app_runtime {

// CallbackHelper is used to hide base::OnceCallback to WAM.
//
// Define your OnceCallback type to expose to WAM
// using MyCallback = CallbackHelper<void(bool)>;
//
// Wrap OnceCallback to CallbackHelper::Callback and make MyCallback
// void SetResut(bool result);
// MyCallback
// cb(std::unique_ptr<WrapOnceCallback<void(bool)>>(base::BindOnce(&SetResult)));
//
// Note.
// MyCallback can not be copied like OnceCallback and must be called like
// std::move(cb).Run(value)

template <typename Signature>
class CallbackHelper;
template <typename R, typename... Args>
class CallbackHelper<R(Args...)> {
 public:
  using RunType = R(Args...);

  struct Callback {
    virtual ~Callback() = default;
    virtual void operator()(Args...);
  };

  CallbackHelper() = default;
  CallbackHelper(std::unique_ptr<Callback> callback)
      : callback_(std::move(callback)) {}

  CallbackHelper(const CallbackHelper&) = delete;
  CallbackHelper& operator=(const CallbackHelper&) = delete;

  CallbackHelper(CallbackHelper&&) noexcept = default;
  CallbackHelper& operator=(CallbackHelper&&) noexcept = default;

  void Run(Args... args) const& {
    static_assert(!sizeof(*this),
                  "CallbackHelper::Run() may only be invoked on a non-const "
                  "rvalue, i.e. std::move(callback).Run().");
  }

  void Run(Args... args) && {
    CallbackHelper cb = std::move(*this);

    if (cb.callback_) {
      (*cb.callback_)(std::forward<Args>(args)...);
    }
  }

 private:
  std::unique_ptr<Callback> callback_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_PUBLIC_CALLBACK_HELPER_H_
