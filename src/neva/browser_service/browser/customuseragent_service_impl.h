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

#ifndef NEVA_BROWSER_SERVICE_BROWSER_CUSTOM_USERAGENT_SERVICE_IMPL_H_
#define NEVA_BROWSER_SERVICE_BROWSER_CUSTOM_USERAGENT_SERVICE_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "neva/browser_service/public/mojom/customuseragent_service.mojom.h"
#include "url/gurl.h"

namespace browser {

class CustomUserAgentServiceImpl : public mojom::CustomUserAgentService {
 public:
  static CustomUserAgentServiceImpl* Get();

  void AddBinding(
      mojo::PendingReceiver<mojom::CustomUserAgentService> receiver);

  void GetServerCredentials(GetServerCredentialsCallback callback) override;
  void CreateEncryptedServerCredentials(
      const std::string& server_input_data,
      const std::string& cipher_key,
      CreateEncryptedServerCredentialsCallback callback) override;

 private:
  friend struct base::DefaultSingletonTraits<CustomUserAgentServiceImpl>;

  CustomUserAgentServiceImpl() = default;
  CustomUserAgentServiceImpl(const CustomUserAgentServiceImpl&) = delete;
  CustomUserAgentServiceImpl& operator=(const CustomUserAgentServiceImpl&) =
      delete;
  ~CustomUserAgentServiceImpl() = default;

  const std::string ReadDataFromFile(const char* file_name);
  const base::FilePath GetFilePath(const char* file_name);
  const std::string GetServerInfoFromDisk();
  std::string GetEncryptedDataByKey(const std::string& encript_key,
                                    const std::string& data);

  std::string server_info_;
  mojo::ReceiverSet<mojom::CustomUserAgentService> receivers_;
};

}  // namespace browser

#endif  // NEVA_BROWSER_SERVICE_BROWSER_CUSTOM_USERAGENT_SERVICE_H_