// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"

#include "chrome/browser/touch_to_fill/payments/android/jni_headers/TouchToFillCreditCardControllerBridge_jni.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"

namespace autofill {

TouchToFillCreditCardController::TouchToFillCreditCardController() = default;
TouchToFillCreditCardController::~TouchToFillCreditCardController() {
  if (java_object_) {
    Java_TouchToFillCreditCardControllerBridge_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
}

bool TouchToFillCreditCardController::Show(
    std::unique_ptr<TouchToFillCreditCardView> view,
    base::WeakPtr<TouchToFillDelegate> delegate) {
  // Abort if TTF surface is already shown.
  if (view_)
    return false;

  if (!view->Show(this)) {
    java_object_.Reset();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  SetShouldSuppressKeyboard(/*suppress=*/true);
  return true;
}

void TouchToFillCreditCardController::Hide() {
  if (view_)
    view_->Hide();
}

void TouchToFillCreditCardController::SetShouldSuppressKeyboard(bool suppress) {
  if (delegate_) {
    static_cast<ContentAutofillDriver*>(delegate_->GetDriver())
        ->SetShouldSuppressKeyboard(suppress);
  }
}

void TouchToFillCreditCardController::OnDismissed(JNIEnv* env) {
  SetShouldSuppressKeyboard(/*suppress=*/false);
  view_.reset();
  delegate_.reset();
  java_object_.Reset();
}

base::android::ScopedJavaLocalRef<jobject>
TouchToFillCreditCardController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_TouchToFillCreditCardControllerBridge_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

}  // namespace autofill
