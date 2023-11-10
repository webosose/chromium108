// Copyright 2023 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <iostream>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/os_crypt/os_crypt.h"
#include "components/url_formatter/url_fixer.h"
#include "content/shell/common/shell_neva_switches.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "neva/browser_service/browser/customuseragent_service_impl.h"

namespace browser {

namespace {

const char kCustomUAAuthDecryptionKey[] = "custom_ua_auth_decryption_key";
const char kCustomUAServiceAuthData[] = "custom_ua_service_auth_data";
const char kInitializationVector[] = "0123456789ABCDEF";

}  // namespace

CustomUserAgentServiceImpl* CustomUserAgentServiceImpl::Get() {
  return base::Singleton<CustomUserAgentServiceImpl>::get();
}

void CustomUserAgentServiceImpl::AddBinding(
    mojo::PendingReceiver<mojom::CustomUserAgentService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CustomUserAgentServiceImpl::GetServerCredentials(
    GetServerCredentialsCallback callback) {
  if (server_info_.empty()) {
    server_info_ = GetServerInfoFromDisk();
    if (server_info_.empty()) {
      LOG(ERROR) << __func__ << " Failed to get API key for custom UA feature";
    }
  }

  std::move(callback).Run(server_info_);
}

void CustomUserAgentServiceImpl::CreateEncryptedServerCredentials(
    const std::string& server_input_data,
    const std::string& cipher_key,
    CreateEncryptedServerCredentialsCallback callback) {
  std::string encrypt_data =
      GetEncryptedDataByKey(cipher_key, server_input_data);
  std::move(callback).Run(encrypt_data);
}

const base::FilePath CustomUserAgentServiceImpl::GetFilePath(
    const char* file_name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  base::FilePath file_path(cmd_line->GetSwitchValuePath(switches::kUserDataDir)
                               .AppendASCII(file_name));
  return file_path;
}

const std::string CustomUserAgentServiceImpl::ReadDataFromFile(
    const char* file_name) {
  std::string data;
  if (!base::ReadFileToString(GetFilePath(file_name), &data)) {
    VLOG(2) << __func__ << " Failed to read from File !!";
    return {};
  }
  // Trim data for whitespaces, CR, LF
  return base::UTF16ToUTF8(
      base::TrimWhitespace(base::UTF8ToUTF16(data), base::TRIM_ALL));
}

std::string CustomUserAgentServiceImpl::GetEncryptedDataByKey(
    const std::string& cipher_key,
    const std::string& data) {
  std::string encrypted_auth_key;
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, cipher_key);
  if (!key) {
    LOG(ERROR) << __func__ << " Failed to import key for AES encryption.";
    return encrypted_auth_key;
  }

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CTR, "")) {
    LOG(ERROR) << __func__ << " Failed to initialize AES-CTR Encryptor.";
    return encrypted_auth_key;
  }

  if (!encryptor.SetCounter(kInitializationVector)) {
    LOG(ERROR) << __func__ << " Could not set counter block.";
    return encrypted_auth_key;
  }

  if (!encryptor.Encrypt(data, &encrypted_auth_key)) {
    LOG(ERROR) << __func__ << " Encryption Failed, Invalid key !!";
    return std::string();
  }

  std::string b64_decrypted_key;
  base::Base64Encode(encrypted_auth_key, &b64_decrypted_key);
  return b64_decrypted_key;
}

const std::string CustomUserAgentServiceImpl::GetServerInfoFromDisk() {
  std::string decrypted_auth_data;

  // Read the AES128-CTR encryption key from file
  const std::string cipher_key = ReadDataFromFile(kCustomUAAuthDecryptionKey);
  if (cipher_key.empty()) {
    LOG(ERROR) << __func__ << " Empty Cipher key !";
    return decrypted_auth_data;
  }

  // Read the encrypted authentication data from file
  const std::string encrypted_data = ReadDataFromFile(kCustomUAServiceAuthData);
  if (encrypted_data.empty()) {
    LOG(ERROR) << __func__ << " Empty User Agent authentication Data !";
    return decrypted_auth_data;
  }
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, cipher_key);
  if (!key) {
    LOG(ERROR) << __func__ << " Failed to import key for AES encryption.";
    return decrypted_auth_data;
  }

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CTR, "")) {
    LOG(ERROR) << __func__ << " Failed to initialize AES-CTR Encryptor.";
    return decrypted_auth_data;
  }

  if (!encryptor.SetCounter(kInitializationVector)) {
    LOG(ERROR) << __func__ << " Could not set counter block.";
    return decrypted_auth_data;
  }

  std::string b64_encrypted_data;
  base::Base64Decode(encrypted_data, &b64_encrypted_data);

  if (!encryptor.Decrypt(b64_encrypted_data, &decrypted_auth_data)) {
    LOG(ERROR) << __func__ << " Decryption Failed, Invalid key !!";
    return std::string();
  }

  return decrypted_auth_data;
}

}  // namespace browser
