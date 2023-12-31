// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/auth_factor_input.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user.h"

namespace ash {

using ::cryptohome::KeyLabel;

AuthFactorEditor::AuthFactorEditor() = default;
AuthFactorEditor::~AuthFactorEditor() = default;

void AuthFactorEditor::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthFactorEditor> AuthFactorEditor::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthFactorEditor::GetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Listing AuthFactors";
  user_data_auth::ListAuthFactorsRequest request;

  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  UserDataAuthClient::Get()->ListAuthFactors(
      request, base::BindOnce(&AuthFactorEditor::OnListAuthFactors,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddKioskKey(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback) {
  if (features::IsUseAuthFactorsEnabled()) {
    const SessionAuthFactors& auth_factors = context->GetAuthFactorsData();
    auto* existing_factor = auth_factors.FindKioskFactor();
    if (existing_factor != nullptr) {
      LOGIN_LOG(ERROR) << "Adding Kiosk key while one already exists";
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{
              user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED});
      return;
    }

    LOGIN_LOG(EVENT) << "Adding Kiosk key";

    user_data_auth::AddAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kKiosk,
                                  KeyLabel{kCryptohomePublicMountLabel}};
    cryptohome::AuthFactorCommonMetadata metadata;
    cryptohome::AuthFactor factor(ref, std::move(metadata));

    cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::Kiosk{});
    cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    UserDataAuthClient::Get()->AddAuthFactor(
        request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }
  const SessionAuthFactors& auth_factors = context->GetAuthFactorsData();
  if (auto* key_def = auth_factors.FindKioskKey()) {
    LOGIN_LOG(ERROR) << "Adding Kiosk key while one already exists";
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED});
    return;
  }

  LOGIN_LOG(EVENT) << "Adding Kiosk key";
  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::KeyData* key_data =
      request.mutable_authorization()->mutable_key()->mutable_data();
  key_data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
  key_data->set_label(kCryptohomePublicMountLabel);

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    if (context->GetKey()->GetLabel().empty()) {
      context->GetKey()->SetLabel(kCryptohomeGaiaKeyLabel);
    }  // empty label
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthFactorEditor::HashContextKeyAndAdd, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  LOGIN_LOG(EVENT) << "Adding knowledge key from the context "
                   << context->GetKey()->GetKeyType();

  if (features::IsUseAuthFactorsEnabled()) {
    auto* key = context->GetKey();
    DCHECK(!key->GetLabel().empty());

    user_data_auth::AddAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    // For now convert Key structure.
    if (context->IsUsingPin()) {
      cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                    KeyLabel{key->GetLabel()}};
      cryptohome::AuthFactorCommonMetadata metadata;
      cryptohome::AuthFactor factor(ref, std::move(metadata));

      cryptohome::AuthFactorInput input(
          cryptohome::AuthFactorInput::Pin{key->GetSecret()});
      cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
      cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    } else {
      cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                    KeyLabel{key->GetLabel()}};
      cryptohome::AuthFactorCommonMetadata metadata;
      cryptohome::AuthFactor factor(ref, std::move(metadata));

      cryptohome::AuthFactorInput input(
          cryptohome::AuthFactorInput::Password{key->GetSecret()});
      cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
      cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    }
    UserDataAuthClient::Get()->AddAuthFactor(
        request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }  // IsUseAuthFactorsEnabled()
  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::Key* key = request.mutable_authorization()->mutable_key();
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(*context), key);

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());

  LOGIN_LOG(EVENT) << "Adding challenge-response key from the context";
  if (features::IsUseAuthFactorsEnabled()) {
    user_data_auth::AddAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    DCHECK_EQ(context->GetChallengeResponseKeys().size(), 1ull);

    auto label =
        GenerateChallengeResponseKeyLabel(context->GetChallengeResponseKeys());
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kSmartCard,
                                  cryptohome::KeyLabel{label}};
    cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::SmartCard{
        context->GetChallengeResponseKeys()[0].signature_algorithms(),
        cryptohome::kCryptohomeKeyDelegateServiceName,
    });

    cryptohome::SmartCardMetadata smart_card_metadata{
        context->GetChallengeResponseKeys()[0].public_key_spki_der()};

    cryptohome::AuthFactorCommonMetadata metadata;
    cryptohome::AuthFactor factor(ref, std::move(metadata),
                                  std::move(smart_card_metadata));

    cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

    UserDataAuthClient::Get()->AddAuthFactor(
        request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }

  user_data_auth::AddCredentialsRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  // Note that we need to specify key_delegate even when we set up a factor,
  // not only when we use it.
  *request.mutable_authorization() = CreateAuthorizationRequestFromKeyDef(
      cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
          *context));

  UserDataAuthClient::Get()->AddCredentials(
      request, base::BindOnce(&AuthFactorEditor::OnAddCredentials,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::HashContextKeyAndAdd(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AddContextKnowledgeKey(std::move(context), std::move(callback));
}

void AuthFactorEditor::ReplaceContextKey(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());
  DCHECK(context->HasReplacementKey());
  DCHECK(!context->IsUsingPin());
  DCHECK_NE(context->GetReplacementKey()->GetKeyType(),
            Key::KEY_TYPE_PASSWORD_PLAIN);

  LOGIN_LOG(EVENT) << "Replacing key from context "
                   << context->GetReplacementKey()->GetKeyType();

  if (features::IsUseAuthFactorsEnabled()) {
    user_data_auth::UpdateAuthFactorRequest request;
    request.set_auth_session_id(context->GetAuthSessionId());

    auto* old_key = context->GetKey();
    auto* key = context->GetReplacementKey();

    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                  KeyLabel{old_key->GetLabel()}};

    request.set_auth_factor_label(ref.label().value());

    cryptohome::AuthFactorCommonMetadata metadata;
    cryptohome::AuthFactor factor(ref, std::move(metadata));

    cryptohome::AuthFactorInput input(
        cryptohome::AuthFactorInput::Password{key->GetSecret()});
    cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    UserDataAuthClient::Get()->UpdateAuthFactor(
        request, base::BindOnce(&AuthFactorEditor::OnUpdateAuthFactor,
                                weak_factory_.GetWeakPtr(), std::move(context),
                                std::move(callback)));
    return;
  }
  user_data_auth::UpdateCredentialRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_old_credential_label(context->GetKey()->GetLabel());
  const Key* key = context->GetReplacementKey();
  cryptohome::KeyDefinitionToKey(
      cryptohome::KeyDefinition::CreateForPassword(key->GetSecret(),
                                                   KeyLabel(key->GetLabel()),
                                                   cryptohome::PRIV_DEFAULT),
      request.mutable_authorization()->mutable_key());

  UserDataAuthClient::Get()->UpdateCredential(
      request, base::BindOnce(&AuthFactorEditor::OnUpdateCredential,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddPinFactor(std::unique_ptr<UserContext> context,
                                    cryptohome::PinSalt salt,
                                    cryptohome::RawPin pin,
                                    AuthOperationCallback callback) {
  DCHECK(features::IsUseAuthFactorsEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                KeyLabel{kCryptohomePinLabel}};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata));

  Key key{std::move(*pin)};
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, std::move(*salt));

  const cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Pin{key.GetSecret()});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto on_added_callback = base::BindOnce(
      &AuthFactorEditor::OnAddAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  LOGIN_LOG(EVENT) << "Adding pin factor";
  UserDataAuthClient::Get()->AddAuthFactor(std::move(request),
                                           std::move(on_added_callback));
}

void AuthFactorEditor::ReplacePinFactor(std::unique_ptr<UserContext> context,
                                        cryptohome::PinSalt salt,
                                        cryptohome::RawPin pin,
                                        AuthOperationCallback callback) {
  DCHECK(features::IsUseAuthFactorsEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  user_data_auth::UpdateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  request.set_auth_factor_label(kCryptohomePinLabel);

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                KeyLabel{kCryptohomePinLabel}};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata));

  Key key{std::move(*pin)};
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, std::move(*salt));

  const cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Pin{key.GetSecret()});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto on_updated_callback = base::BindOnce(
      &AuthFactorEditor::OnUpdateAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  LOGIN_LOG(EVENT) << "Replacing pin factor";
  UserDataAuthClient::Get()->UpdateAuthFactor(std::move(request),
                                              std::move(on_updated_callback));
}

void AuthFactorEditor::RemovePinFactor(std::unique_ptr<UserContext> context,
                                       AuthOperationCallback callback) {
  DCHECK(features::IsUseAuthFactorsEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  LOGIN_LOG(EVENT) << "Removing pin factor";

  user_data_auth::RemoveAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());
  req.set_auth_factor_label(kCryptohomePinLabel);

  auto remove_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRemoveAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  UserDataAuthClient::Get()->RemoveAuthFactor(
      req, std::move(remove_auth_factor_callback));
}

void AuthFactorEditor::AddRecoveryFactor(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  CHECK(features::IsCryptohomeRecoverySetupEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/1310312): Check whether a recovery key already exists and
  // return immediately.

  LOGIN_LOG(EVENT) << "Adding recovery key";

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kRecovery,
                                KeyLabel{kCryptohomeRecoveryKeyLabel}};
  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata));

  // TODO(crbug.com/1310312): The public key will likely be hardcoded, although
  //  perhaps configurable via a command line switch for testing.
  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::RecoveryCreation{"STUB MEDIATOR PUB KEY"});

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto add_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnAddAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));

  UserDataAuthClient::Get()->AddAuthFactor(std::move(request),
                                           std::move(add_auth_factor_callback));
}

void AuthFactorEditor::RemoveRecoveryFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  CHECK(features::IsCryptohomeRecoverySetupEnabled());
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/1310312): Check whether a recovery key already exists and
  // return immediately if there are no recovery keys.

  LOGIN_LOG(EVENT) << "Removing recovery key";

  user_data_auth::RemoveAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());
  req.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  auto remove_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRemoveAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  UserDataAuthClient::Get()->RemoveAuthFactor(
      req, std::move(remove_auth_factor_callback));
}

/// ---- private callbacks ----

void AuthFactorEditor::OnListAuthFactors(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::ListAuthFactorsReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Could not list auth factors " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Got list of configured auth factors";
  DCHECK(features::IsUseAuthFactorsEnabled());

  std::vector<cryptohome::AuthFactor> factor_list;
  cryptohome::AuthFactorType fallback_type =
      cryptohome::AuthFactorType::kPassword;
  if (user_manager::User::TypeIsKiosk(context->GetUserType()))
    fallback_type = cryptohome::AuthFactorType::kKiosk;
  for (const auto& factor_with_status_proto :
       reply->configured_auth_factors_with_status()) {
    auto factor = cryptohome::DeserializeAuthFactor(
        factor_with_status_proto.auth_factor(), fallback_type);
    // Dirty hack below, as cryptohome does not send correct value as a part of
    // PIN status, but uses indirect signal of listing no intents instead.
    if (factor.ref().type() == cryptohome::AuthFactorType::kPin) {
      bool locked = factor_with_status_proto.available_for_intents_size() == 0;
      cryptohome::PinStatus replacment_status{locked};
      cryptohome::AuthFactor replacement_factor{
          factor.ref(), factor.GetCommonMetadata(), replacment_status};
      factor = replacement_factor;
    }
    factor_list.emplace_back(std::move(factor));
  }
  cryptohome::AuthFactorsSet supported_factors;
  for (const auto proto_type : reply->supported_auth_factors()) {
    cryptohome::AuthFactorType type = cryptohome::ConvertFactorTypeFromProto(
        static_cast<user_data_auth::AuthFactorType>(proto_type));
    if (type == cryptohome::AuthFactorType::kUnknownLegacy) {
      NOTREACHED();
      continue;
    }
    supported_factors.Put(type);
  }

  AuthFactorsConfiguration configured_factors(std::move(factor_list),
                                              supported_factors);
  context->SetAuthFactorsConfiguration(std::move(configured_factors));
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthFactorEditor::OnAddCredentials(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AddCredentialsReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AddCredentials failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully added credentials";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), absl::nullopt);
  // TODO(crbug.com/1310312): Think if we should update SessionAuthFactors in
  // context after such operation.
}

void AuthFactorEditor::OnAddAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AddAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AddAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully added auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), absl::nullopt);
  // TODO(crbug.com/1310312): Think if we should update SessionAuthFactors in
  // context after such operation.
}

void AuthFactorEditor::OnUpdateCredential(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::UpdateCredentialReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "UpdateCredential failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully updated credential";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthFactorEditor::OnUpdateAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::UpdateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "UpdateAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully updated auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthFactorEditor::OnRemoveAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<::user_data_auth::RemoveAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "RemoveAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully removed auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
