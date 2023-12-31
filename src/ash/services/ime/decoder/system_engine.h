// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
#define ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_

#include "ash/services/ime/ime_decoder.h"
#include "ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "ash/services/ime/public/mojom/connection_factory.mojom.h"
#include "ash/services/ime/public/mojom/input_engine.mojom.h"
#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "base/scoped_native_library.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace ime {

// An enhanced implementation of the basic InputEngine that uses a built-in
// shared library for handling key events.
// TODO(b/214153032): Rename to MojoModeSharedLibEngine, and maybe also unnest
// out of "decoder" sub-directory, to better reflect what this represents. This
// class actually wraps MojoMode "C" API entry points of the loaded CrOS 1P IME
// shared lib, to facilitate accessing an IME engine therein via MojoMode.
class SystemEngine {
 public:
  explicit SystemEngine(ImeCrosPlatform* platform,
                        absl::optional<ImeDecoder::EntryPoints> entry_points);

  SystemEngine(const SystemEngine&) = delete;
  SystemEngine& operator=(const SystemEngine&) = delete;
  ~SystemEngine();

  // Binds the mojom::InputMethod interface to this object and returns true if
  // the given ime_spec is supported by the engine.
  bool BindRequest(const std::string& ime_spec,
                   mojo::PendingReceiver<mojom::InputMethod> receiver,
                   mojo::PendingRemote<mojom::InputMethodHost> host);

  // Binds the mojom::ConnectionFactory interface in the shared library.
  bool BindConnectionFactory(
      mojo::PendingReceiver<mojom::ConnectionFactory> receiver);

  bool IsConnected();

 private:
  absl::optional<ImeDecoder::EntryPoints> decoder_entry_points_;
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
