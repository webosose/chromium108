// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_VI_VIQR_H_
#define ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_VI_VIQR_H_

namespace vi_viqr {

// The id of this IME/keyboard.
extern const char* kId;

// The transform rules definition. The string items in the even indexes are
// the regular expressions represent what needs to be transformed, and the
// ones in the odd indexes represent it can transform to what.
extern const char* kTransforms[];

// The length of the transform rules.
extern const unsigned int kTransformsLen;

// The history prune regexp.
extern const char* kHistoryPrune;

}  // namespace vi_viqr

#endif  // ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_VI_VIQR_H_
