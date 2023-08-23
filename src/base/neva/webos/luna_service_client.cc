// Copyright 2018 LG Electronics, Inc.
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

#include "base/neva/webos/luna_service_client.h"

#include <glib.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"

namespace base {

namespace {

const char kURIAudio[] = "luna://com.webos.service.audio";
const char kURISetting[] = "luna://com.webos.settingsservice";
const char kURIMediaController[] = "luna://com.webos.service.mediacontroller";
const char kURICamera[] = "luna://com.webos.service.camera2";

const std::string kLunaClientNameMedia = "com.webos.media.client";

std::string ClientTypeToClientName(LunaServiceClient::ClientType type) {
  if (type == LunaServiceClient::ClientType::MEDIA)
    return kLunaClientNameMedia;

  LOG(ERROR) << __func__ << " Cannot convert";
  return std::string();
}

struct AutoLSError : LSError {
  AutoLSError() { LSErrorInit(this); }
  ~AutoLSError() { LSErrorFree(this); }
};

void LogError(const std::string& message, const AutoLSError& lserror) {
  LOG(ERROR) << message << " " << lserror.error_code << " : " << lserror.message
             << "(" << lserror.func << " @ " << lserror.file << ":"
             << lserror.line << ")";
}

}  // namespace

// static
std::string LunaServiceClient::GetServiceURI(URIType type,
                                             const std::string& action) {
  if (type < 0 || type > URIType::URITypeMax)
    return std::string();

  static std::map<URIType, std::string> kURIMap = {
      {URIType::AUDIO, kURIAudio},
      {URIType::SETTING, kURISetting},
      {URIType::MEDIACONTROLLER, kURIMediaController},
      {URIType::CAMERA, kURICamera},
  };
  auto luna_service_uri = [&type]() {
    std::map<URIType, std::string>::iterator it;
    it = kURIMap.find(type);
    if (it != kURIMap.end())
      return it->second;
    return std::string();
  };

  std::string uri = luna_service_uri();
  uri.append("/");
  uri.append(action);
  return uri;
}

// LunaServiceClient implematation
LunaServiceClient::LunaServiceClient(ClientType type) {
  Initialize(ClientTypeToClientName(type), false);
}

LunaServiceClient::LunaServiceClient(const std::string& identifier,
                                     bool application_service) {
  Initialize(identifier, application_service);
}

LunaServiceClient::~LunaServiceClient() {
  UnregisterService();
}

bool HandleAsync(LSHandle* sh, LSMessage* reply, void* ctx) {
  LunaServiceClient::ResponseHandlerWrapper* wrapper =
      static_cast<LunaServiceClient::ResponseHandlerWrapper*>(ctx);

  LSMessageRef(reply);
  std::string dump = LSMessageGetPayload(reply);
  VLOG(1) << __func__ << "[RES] - " << wrapper->uri << " " << dump;
  if (!wrapper->callback.is_null())
    std::move(wrapper->callback).Run(dump);

  LSMessageUnref(reply);

  delete wrapper;
  return true;
}

bool HandleSubscribe(LSHandle* sh, LSMessage* reply, void* ctx) {
  LunaServiceClient::ResponseHandlerWrapper* wrapper =
      static_cast<LunaServiceClient::ResponseHandlerWrapper*>(ctx);

  LSMessageRef(reply);
  std::string dump = LSMessageGetPayload(reply);
  VLOG(1) << __func__ << "[SUB-RES] - " << wrapper->uri << " " << dump;
  if (!wrapper->callback.is_null())
    wrapper->callback.Run(dump);

  LSMessageUnref(reply);

  return true;
}

bool LunaServiceClient::CallAsync(const std::string& uri,
                                  const std::string& param) {
  if (!handle_) {
    LOG(ERROR) << __func__ << " LSHandle not initialized";
    return false;
  }

  ResponseCB nullcb;
  return CallAsync(uri, param, nullcb);
}

bool LunaServiceClient::CallAsync(const std::string& uri,
                                  const std::string& param,
                                  const ResponseCB& callback) {
  if (!handle_) {
    LOG(ERROR) << __func__ << " LSHandle not initialized";
    return false;
  }

  AutoLSError error;
  ResponseHandlerWrapper* wrapper = new ResponseHandlerWrapper;
  if (!wrapper) {
    LOG(ERROR) << __func__ << " Failed to allocate wrapper";
    return false;
  }

  wrapper->callback = callback;
  wrapper->uri = uri;
  wrapper->param = param;

  VLOG(1) << __func__ << "[REQ] - " << uri << " " << param;
  if (!LSCallOneReply(handle_, uri.c_str(), param.c_str(), HandleAsync, wrapper,
                      nullptr, &error)) {
    LOG(ERROR) << __func__ << " " << uri << ": fail[" << error.message << "]";
    std::move(wrapper->callback).Run("");
    delete wrapper;
    return false;
  }

  return true;
}

bool LunaServiceClient::Subscribe(const std::string& uri,
                                  const std::string& param,
                                  LSMessageToken* subscribe_key,
                                  const ResponseCB& callback) {
  if (!handle_) {
    LOG(ERROR) << __func__ << " LSHandle not initialized";
    return false;
  }

  AutoLSError error;
  ResponseHandlerWrapper* wrapper = new ResponseHandlerWrapper;
  if (!wrapper)
    return false;

  wrapper->callback = callback;
  wrapper->uri = uri;
  wrapper->param = param;

  VLOG(1) << "[REQ] - " << uri << " " << param;
  if (!LSCall(handle_, uri.c_str(), param.c_str(), HandleSubscribe, wrapper,
              subscribe_key, &error)) {
    LOG(ERROR) << __func__ << "  " << uri << ": fail[" << error.message << "]";
    delete wrapper;
    return false;
  }

  handlers_[*subscribe_key] = std::unique_ptr<ResponseHandlerWrapper>(wrapper);

  return true;
}

bool LunaServiceClient::Unsubscribe(LSMessageToken subscribe_key) {
  AutoLSError error;

  if (!handle_)
    return false;

  if (!LSCallCancel(handle_, subscribe_key, &error)) {
    LOG(ERROR) << __func__ << " " << subscribe_key << " [" << error.message
               << "]";
    handlers_.erase(subscribe_key);
    return false;
  }

  if (handlers_[subscribe_key])
    handlers_[subscribe_key]->callback.Reset();

  handlers_.erase(subscribe_key);

  return true;
}

void LunaServiceClient::Initialize(const std::string& identifier,
                                   bool application_service) {
  bool success = false;
  if (application_service) {
    success = RegisterApplicationService(identifier);
  } else
    success = RegisterService(identifier);

  if (success && handle_) {
    AutoLSError error;
    context_ = g_main_context_ref(g_main_context_default());
    if (!LSGmainContextAttach(handle_, context_, &error)) {
      UnregisterService();
      LogError("Fail to attach a service to a mainloop", error);
    }
  } else {
    LOG(ERROR) << __func__ << "Failed to register: " << identifier.c_str();
  }
}

bool LunaServiceClient::RegisterApplicationService(const std::string& appid) {
  std::string name = appid + '-' + std::to_string(getpid());
  AutoLSError error;
  if (!LSRegisterApplicationService(name.c_str(), appid.c_str(), &handle_,
                                    &error)) {
    LogError("Fail to register to LS2", error);
    return false;
  }
  return true;
}

bool LunaServiceClient::RegisterService(const std::string& appid) {
  std::string name = appid;
  if (!name.empty() && *name.rbegin() != '.' && *name.rbegin() != '-')
    name += "-";

  // Some clients may have connection with empty identifier.
  // So append random number only for non empty identifier.
  if (!name.empty())
    name += std::to_string(getpid());

  AutoLSError error;
  if (!LSRegister(name.c_str(), &handle_, &error)) {
    LogError("Fail to register to LS2", error);
    return false;
  }
  return true;
}

bool LunaServiceClient::UnregisterService() {
  AutoLSError error;
  if (!handle_)
    return false;
  if (!LSUnregister(handle_, &error)) {
    g_main_context_unref(context_);
    context_ = nullptr;
    LogError("Fail to unregister service", error);
    return false;
  }
  handle_ = nullptr;
  g_main_context_unref(context_);
  context_ = nullptr;
  return true;
}

}  // namespace base
