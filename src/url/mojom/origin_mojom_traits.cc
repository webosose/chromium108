// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/mojom/origin_mojom_traits.h"

#include "base/strings/string_piece.h"

namespace mojo {

// static
bool StructTraits<url::mojom::OriginDataView, url::Origin>::Read(
    url::mojom::OriginDataView data,
    url::Origin* out) {
  base::StringPiece scheme, host;
  absl::optional<base::UnguessableToken> nonce_if_opaque;
  if (!data.ReadScheme(&scheme) || !data.ReadHost(&host) ||
      !data.ReadNonceIfOpaque(&nonce_if_opaque))
    return false;

#if defined(USE_NEVA_APPRUNTIME)
  absl::optional<std::string> webapp_id;
  if (!data.ReadWebappId(&webapp_id))
    return false;
#endif

  absl::optional<url::Origin> creation_result =
      nonce_if_opaque
          ? url::Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
                scheme, host, data.port(), url::Origin::Nonce(*nonce_if_opaque))
          : url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
                scheme, host, data.port());
  if (!creation_result)
    return false;

  *out = std::move(creation_result.value());

#if defined(USE_NEVA_APPRUNTIME)
  if (webapp_id)
    out->set_webapp_id(*webapp_id);
#endif
  return true;
}

}  // namespace mojo
