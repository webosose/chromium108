// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/syslog_logging.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "crypto/unexportable_key.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {

constexpr int kMaxDMTokenLength = 4096;

BPKUR::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return BPKUR::RSA_KEY;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return BPKUR::EC_KEY;
  }
}

}  // namespace

KeyRotationManagerImpl::KeyRotationManagerImpl(
    std::unique_ptr<KeyNetworkDelegate> network_delegate,
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate)
    : network_delegate_(std::move(network_delegate)),
      persistence_delegate_(std::move(persistence_delegate)) {
  DCHECK(network_delegate_);
  DCHECK(persistence_delegate_);

  key_pair_ = persistence_delegate_->LoadKeyPair();
}

KeyRotationManagerImpl::~KeyRotationManagerImpl() = default;

void KeyRotationManagerImpl::Rotate(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const std::string& nonce,
    base::OnceCallback<void(Result)> result_callback) {
  if (dm_token.size() > kMaxDMTokenLength) {
    SYSLOG(ERROR) << "DMToken length out of bounds";
    std::move(result_callback).Run(Result::FAILED);
    return;
  }

  if (!persistence_delegate_->CheckRotationPermissions()) {
    RecordRotationStatus(nonce,
                         RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS);
    std::move(result_callback).Run(Result::FAILED);
    return;
  }

  auto new_key_pair = persistence_delegate_->CreateKeyPair();
  if (!new_key_pair || new_key_pair->is_empty()) {
    RecordRotationStatus(nonce,
                         RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not generate a "
                     "new signing key.";
    std::move(result_callback).Run(Result::FAILED);
    return;
  }

  if (!persistence_delegate_->StoreKeyPair(
          new_key_pair->trust_level(), new_key_pair->key()->GetWrappedKey())) {
    RecordRotationStatus(nonce, RotationStatus::FAILURE_CANNOT_STORE_KEY);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not write to "
                     "signing key storage.";
    std::move(result_callback).Run(Result::FAILED);
    return;
  }

  enterprise_management::DeviceManagementRequest request;
  if (!BuildUploadPublicKeyRequest(
          *new_key_pair, nonce,
          request.mutable_browser_public_key_upload_request())) {
    RecordRotationStatus(nonce, RotationStatus::FAILURE_CANNOT_BUILD_REQUEST);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not build the "
                     "upload key request.";
    std::move(result_callback).Run(Result::FAILED);
    return;
  }

  std::string request_str;
  request.SerializeToString(&request_str);

  // Any attempt to reuse a nonce will result in an INVALID_SIGNATURE error
  // being returned by the server.
  auto upload_key_callback = base::BindOnce(
      &KeyRotationManagerImpl::OnDmServerResponse, weak_factory_.GetWeakPtr(),
      nonce, std::move(new_key_pair), std::move(result_callback));
  network_delegate_->SendPublicKeyToDmServer(
      dm_server_url, dm_token, request_str, std::move(upload_key_callback));
}

bool KeyRotationManagerImpl::BuildUploadPublicKeyRequest(
    const SigningKeyPair& new_key_pair,
    const std::string& nonce,
    enterprise_management::BrowserPublicKeyUploadRequest* request) {
  std::vector<uint8_t> pubkey = new_key_pair.key()->GetSubjectPublicKeyInfo();

  // Build the buffer to sign.  It consists of the public key of the new key
  // pair followed by the nonce.  The nonce vector may be empty.
  std::vector<uint8_t> buffer = pubkey;
  buffer.insert(buffer.end(), nonce.begin(), nonce.end());

  // If there is an existing key and the nonce is not empty, sign the new
  // pubkey with it.  Otherwise sign it with the new key itself (i.e. the
  // public key is self-signed).  This is done to handle the case of a device
  // that is enabled for device trust and then un-enrolled server side.  When
  // the user re-enrolls this device, the first key rotation attempt will use
  // an empty nonce to signal this is the first public key being uploaded to
  // DM server.  DM server expects the public key to be self signed.
  absl::optional<std::vector<uint8_t>> signature =
      key_pair_ && key_pair_->key() && !nonce.empty()
          ? key_pair_->key()->SignSlowly(buffer)
          : new_key_pair.key()->SignSlowly(buffer);
  if (!signature.has_value())
    return false;

  request->set_public_key(pubkey.data(), pubkey.size());
  request->set_signature(signature->data(), signature->size());
  request->set_key_trust_level(new_key_pair.trust_level());
  request->set_key_type(AlgorithmToType(new_key_pair.key()->Algorithm()));

  return true;
}

void KeyRotationManagerImpl::OnDmServerResponse(
    const std::string& nonce,
    std::unique_ptr<SigningKeyPair> new_key_pair,
    base::OnceCallback<void(Result)> result_callback,
    KeyNetworkDelegate::HttpResponseCode response_code) {
  RecordUploadCode(nonce, response_code);
  auto upload_key_status = ParseUploadKeyStatus(response_code);
  if (upload_key_status != UploadKeyStatus::kSucceeded) {
    // Unable to send to DM server, so restore the old key if there was one.
    bool able_to_restore = true;
    if (key_pair_ && key_pair_->key()) {
      able_to_restore = persistence_delegate_->StoreKeyPair(
          key_pair_->trust_level(), key_pair_->key()->GetWrappedKey());
    } else {
      // If there was no old key we clear the registry.
      able_to_restore = persistence_delegate_->StoreKeyPair(
          BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>());
    }

    RotationStatus status =
        able_to_restore
            ? ((upload_key_status == UploadKeyStatus::kFailedRetryable)
                   ? RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY)
            : ((upload_key_status == UploadKeyStatus::kFailedRetryable)
                   ? RotationStatus::
                         FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED
                   : RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED);
    RecordRotationStatus(nonce, status);
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not send public "
                     "key to DM server.";
    if (upload_key_status == UploadKeyStatus::kFailedKeyConflict) {
      std::move(result_callback)
          .Run(KeyRotationManager::Result::FAILED_KEY_CONFLICT);
      return;
    }

    std::move(result_callback).Run(KeyRotationManager::Result::FAILED);
    return;
  }
  persistence_delegate_->CleanupTemporaryKeyData();
  key_pair_ = std::move(new_key_pair);
  RecordRotationStatus(nonce, RotationStatus::SUCCESS);
  std::move(result_callback).Run(KeyRotationManager::Result::SUCCEEDED);
}

}  // namespace enterprise_connectors
