// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on chrome/browser/push_messaging/push_messaging_service_impl.cc
// Rmoved AbusiveOrigin checking becase webOS doesn't use CrowdDeny Component

#include "neva/app_runtime/browser/push_messaging/push_messaging_service_impl.h"

#include <map>
#include <sstream>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "neva/app_runtime/browser/gcm/gcm_profile_service_factory.h"
#include "neva/app_runtime/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "neva/app_runtime/browser/permissions/permission_manager_factory.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_constants.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_features.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_service_factory.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_utils.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace neva_app_runtime {
namespace {
// Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

const int kMaxRegistrations = 1000000;

// Chrome does not yet support silent push messages, and requires websites to
// indicate that they will only send user-visible messages.
const char kSilentPushUnsupportedMessage[] =
    "Chrome currently only supports the Push API for subscriptions that will "
    "result in user-visible messages. You can indicate this by calling "
    "pushManager.subscribe({userVisibleOnly: true}) instead. See "
    "https://goo.gl/yqv4Q4 for more details.";

// Message displayed in the console (as an error) when a GCM Sender ID is used
// to create a subscription, which is unsupported. The subscription request will
// have been blocked, and an exception will be thrown as well.
const char kSenderIdRegistrationDisallowedMessage[] =
    "The provided application server key is not a VAPID key. Only VAPID keys "
    "are supported. For more information check https://crbug.com/979235.";

// Message displayed in the console (as a warning) when a GCM Sender ID is used
// to create a subscription, which will soon be unsupported.
const char kSenderIdRegistrationDeprecatedMessage[] =
    "The provided application server key is not a VAPID key. Only VAPID keys "
    "will be supported in the future. For more information check "
    "https://crbug.com/979235.";

blink::mojom::PermissionStatus ToPermissionStatus(
    ContentSetting content_setting) {
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::mojom::PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return blink::mojom::PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return blink::mojom::PermissionStatus::ASK;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::PermissionStatus::DENIED;
}

void UnregisterCallbackToClosure(
    base::OnceClosure closure,
    blink::mojom::PushUnregistrationStatus status) {
  DCHECK(closure);
  std::move(closure).Run();
}

void LogMessageReceivedEventToDevTools(
    content::DevToolsBackgroundServicesContext* devtools_context,
    const PushMessagingAppIdentifier& app_identifier,
    const std::string& message_id,
    bool was_encrypted,
    const std::string& error_message,
    const std::string& payload) {
  if (!devtools_context)
    return;

  std::map<std::string, std::string> event_metadata = {
      {"Success", error_message.empty() ? "Yes" : "No"},
      {"Was Encrypted", was_encrypted ? "Yes" : "No"}};

  if (!error_message.empty())
    event_metadata["Error Reason"] = error_message;
  else if (was_encrypted)
    event_metadata["Payload"] = payload;

  devtools_context->LogBackgroundServiceEvent(
      app_identifier.service_worker_registration_id(),
      url::Origin::Create(app_identifier.origin()),
      content::DevToolsBackgroundService::kPushMessaging,
      "Push message received" /* event_name */, message_id, event_metadata);
}

content::RenderFrameHost* GetMainFrameForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  return web_contents ? web_contents->GetPrimaryMainFrame() : nullptr;
}

bool IsVapidKey(const std::string& application_server_key) {
  // VAPID keys are NIST P-256 public keys in uncompressed format (64 bytes),
  // verified through its length and the 0x04 prefix.
  return application_server_key.size() == 65 &&
         application_server_key[0] == 0x04;
}

std::string NormalizeSenderInfo(const std::string& application_server_key) {
  if (!IsVapidKey(application_server_key))
    return application_server_key;

  std::string encoded_application_server_key;
  base::Base64UrlEncode(application_server_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_application_server_key);

  return encoded_application_server_key;
}

GURL CreateEndpoint(const std::string& subscription_id) {
  const GURL endpoint(kPushMessagingGcmEndpoint + subscription_id);
  DCHECK(endpoint.is_valid());
  return endpoint;
}

}  // namespace

// static
void PushMessagingServiceImpl::InitializeForProfile(
    content::BrowserContext* context) {
  // TODO(johnme): Consider whether push should be enabled in incognito.
  int count =
      PushMessagingAppIdentifier::GetCount(user_prefs::UserPrefs::Get(context));
  if (count <= 0)
    return;

  PushMessagingServiceImpl* push_service =
      PushMessagingServiceFactory::GetForProfile(context);
  if (push_service) {
    push_service->IncreasePushSubscriptionCount(count, false /* is_pending */);
    push_service->RemoveExpiredSubscriptions();
  }
}

void PushMessagingServiceImpl::RemoveExpiredSubscriptions() {
  if (!base::FeatureList::IsEnabled(
          features::kPushSubscriptionWithExpirationTime)) {
    return;
  }
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      PushMessagingAppIdentifier::GetCount(pref_service_), base::DoNothing());

  for (const auto& identifier :
       PushMessagingAppIdentifier::GetAll(pref_service_)) {
    if (!identifier.IsExpired()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, barrier_closure);
      continue;
    }
    content::BrowserThread::PostBestEffortTask(
        FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
        base::BindOnce(
            &PushMessagingServiceImpl::UnexpectedChange,
            weak_factory_.GetWeakPtr(), identifier,
            blink::mojom::PushUnregistrationReason::SUBSCRIPTION_EXPIRED,
            barrier_closure));
  }
}

void PushMessagingServiceImpl::UnexpectedChange(
    PushMessagingAppIdentifier identifier,
    blink::mojom::PushUnregistrationReason reason,
    base::OnceClosure completed_closure) {
  auto unsubscribe_closure =
      base::BindOnce(&PushMessagingServiceImpl::UnexpectedUnsubscribe,
                     weak_factory_.GetWeakPtr(), identifier, reason,
                     base::BindOnce(&UnregisterCallbackToClosure,
                                    std::move(completed_closure)));
  if (base::FeatureList::IsEnabled(features::kPushSubscriptionChangeEvent)) {
    // Find old subscription and fire a `pushsubscriptionchange` event
    GetPushSubscriptionFromAppIdentifier(
        identifier,
        base::BindOnce(&PushMessagingServiceImpl::FirePushSubscriptionChange,
                       weak_factory_.GetWeakPtr(), identifier,
                       std::move(unsubscribe_closure),
                       nullptr /* new_subscription */));
  } else {
    std::move(unsubscribe_closure).Run();
  }
}

PushMessagingServiceImpl::PushMessagingServiceImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      pref_service_(user_prefs::UserPrefs::Get(browser_context)),
      push_subscription_count_(0),
      pending_push_subscription_count_(0) {}

PushMessagingServiceImpl::~PushMessagingServiceImpl() = default;

void PushMessagingServiceImpl::IncreasePushSubscriptionCount(int add,
                                                             bool is_pending) {
  DCHECK_GT(add, 0);
  if (push_subscription_count_ + pending_push_subscription_count_ == 0) {
    GetGCMDriver()->AddAppHandler(kPushMessagingAppIdentifierPrefix, this);
  }

  if (is_pending)
    pending_push_subscription_count_ += add;
  else
    push_subscription_count_ += add;
}

void PushMessagingServiceImpl::DecreasePushSubscriptionCount(int subtract,
                                                             bool was_pending) {
  DCHECK_GT(subtract, 0);
  if (was_pending) {
    pending_push_subscription_count_ -= subtract;
    DCHECK_GE(pending_push_subscription_count_, 0);
  } else {
    push_subscription_count_ -= subtract;
    DCHECK_GE(push_subscription_count_, 0);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ == 0)
    GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return base::StartsWith(app_id, kPushMessagingAppIdentifierPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED();
}

void PushMessagingServiceImpl::OnStoreReset() {
  // Delete all cached subscriptions, since they are now invalid.
  for (const auto& identifier :
       PushMessagingAppIdentifier::GetAll(pref_service_)) {
    // Clear all the subscriptions in parallel, to reduce risk that shutdown
    // occurs before we finish clearing them.
    ClearPushSubscriptionId(browser_context_, identifier.origin(),
                            identifier.service_worker_registration_id(),
                            base::DoNothing());
    // TODO(johnme): Fire pushsubscriptionchange/pushsubscriptionlost SW event.
  }
  PushMessagingAppIdentifier::DeleteAllFromPrefs(pref_service_);
}

// OnMessage methods -----------------------------------------------------------
void PushMessagingServiceImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {
  LOG(INFO) << __func__ << " app_id=" << app_id
            << " message=" << message.raw_data;
  // We won't have time to process and act on the message.
  // TODO(peter) This should be checked at the level of the GCMDriver, so that
  // the message is not consumed. See https://crbug.com/612815
  // if (g_browser_process->IsShuttingDown() || shutdown_started_)
  //  return;

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(pref_service_, app_id);
  LOG(INFO) << __func__ << " app_id=" << app_identifier.app_id()
            << " origin=" << app_identifier.origin()
            << " sw_reg_id=" << app_identifier.service_worker_registration_id();
  // Drop message and unregister if app_id was unknown (maybe recently deleted).
  if (app_identifier.is_null()) {
    DeliverMessageCallback(app_id, GURL::EmptyGURL(),
                           -1 /* kInvalidServiceWorkerRegistrationId */,
                           message, message_handled_callback(),
                           blink::mojom::PushEventStatus::UNKNOWN_APP_ID);
    return;
  }

  LogMessageReceivedEventToDevTools(
      GetDevToolsContext(app_identifier.origin()), app_identifier,
      message.message_id,
      /* was_encrypted= */ message.decrypted, std::string() /* error_message */,
      message.decrypted ? message.raw_data : std::string());

  if (IsPermissionSet(app_identifier.origin())) {
    absl::optional<std::string> payload;
    if (message.decrypted)
      payload = message.raw_data;
    browser_context_->DeliverPushMessage(
        app_identifier.origin(),
        app_identifier.service_worker_registration_id(), message.message_id,
        payload,
        base::BindOnce(&PushMessagingServiceImpl::DeliverMessageCallback,
                       weak_factory_.GetWeakPtr(), app_identifier.app_id(),
                       app_identifier.origin(),
                       app_identifier.service_worker_registration_id(), message,
                       message_handled_callback()));
  } else {
    // Drop message and unregister if origin has lost push permission.
    DeliverMessageCallback(app_id, app_identifier.origin(),
                           app_identifier.service_worker_registration_id(),
                           message, message_handled_callback(),
                           blink::mojom::PushEventStatus::PERMISSION_DENIED);
  }
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const std::string& app_id,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const gcm::IncomingMessage& message,
    base::OnceClosure message_handled_closure,
    blink::mojom::PushEventStatus status) {
  // A reason to automatically unsubscribe. UNKNOWN means do not unsubscribe.
  blink::mojom::PushUnregistrationReason unsubscribe_reason =
      blink::mojom::PushUnregistrationReason::UNKNOWN;

  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // See https://crbug.com/508516 for options.
  switch (status) {
    // Call EnforceUserVisibleOnlyRequirements if the message was delivered to
    // the Service Worker JavaScript, even if the website's event handler failed
    // (to prevent sites deliberately failing in order to avoid having to show
    // notifications).
    case blink::mojom::PushEventStatus::SUCCESS:
    case blink::mojom::PushEventStatus::EVENT_WAITUNTIL_REJECTED:
    case blink::mojom::PushEventStatus::TIMEOUT:
      // Only enforce the user visible requirements if silent push has not been
      // enabled through a command line flag.
      //
      // TODO(webOS): Enable when notification_manager_ is integrated.
      // if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      //        switches::kAllowSilentPush)) {
      //  notification_manager_.EnforceUserVisibleOnlyRequirements(
      //      requesting_origin, service_worker_registration_id,
      //      std::move(message_handled_callback));
      //  message_handled_callback = base::OnceCallback<void(bool)>();
      //}
      break;
    case blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR:
      // Do nothing, and hope the error is transient.
      break;
    case blink::mojom::PushEventStatus::UNKNOWN_APP_ID:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_UNKNOWN_APP_ID;
      break;
    case blink::mojom::PushEventStatus::PERMISSION_DENIED:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_PERMISSION_DENIED;
      break;
    case blink::mojom::PushEventStatus::NO_SERVICE_WORKER:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_NO_SERVICE_WORKER;
      break;
    case blink::mojom::PushEventStatus::PERMISSION_REVOKED_ABUSIVE:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED_ABUSIVE;
      break;
  }

  // If |message_handled_closure| was not yet used, make a |completion_closure|
  // which should run by default at the end of this function, unless it is
  // explicitly passed to another function or disabled.
  base::ScopedClosureRunner completion_closure_runner(
      message_handled_closure
          ? base::BindOnce(&PushMessagingServiceImpl::DidHandleMessage,
                           weak_factory_.GetWeakPtr(), app_id,
                           message.message_id,
                           std::move(message_handled_closure),
                           false /* did_show_generic_notification */)
          : base::DoNothing());

  if (unsubscribe_reason != blink::mojom::PushUnregistrationReason::UNKNOWN) {
    PushMessagingAppIdentifier app_identifier =
        PushMessagingAppIdentifier::FindByAppId(pref_service_, app_id);
    UnsubscribeInternal(
        unsubscribe_reason,
        app_identifier.is_null() ? GURL::EmptyGURL() : app_identifier.origin(),
        app_identifier.is_null()
            ? -1 /* kInvalidServiceWorkerRegistrationId */
            : app_identifier.service_worker_registration_id(),
        app_id, message.sender_id,
        base::BindOnce(&UnregisterCallbackToClosure,
                       completion_closure_runner.Release()));

    if (app_identifier.is_null())
      return;

    if (auto* devtools_context = GetDevToolsContext(app_identifier.origin())) {
      std::stringstream ss;
      ss << unsubscribe_reason;
      devtools_context->LogBackgroundServiceEvent(
          app_identifier.service_worker_registration_id(),
          url::Origin::Create(app_identifier.origin()),
          content::DevToolsBackgroundService::kPushMessaging,
          "Unsubscribed due to error" /* event_name */, message.message_id,
          {{"Reason", ss.str()}});
    }
  }
}

void PushMessagingServiceImpl::DidHandleMessage(
    const std::string& app_id,
    const std::string& push_message_id,
    base::OnceClosure message_handled_closure,
    bool did_show_generic_notification) {
  std::move(message_handled_closure).Run();

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(pref_service_, app_id);

  if (app_identifier.is_null() || !did_show_generic_notification)
    return;

  if (auto* devtools_context = GetDevToolsContext(app_identifier.origin())) {
    devtools_context->LogBackgroundServiceEvent(
        app_identifier.service_worker_registration_id(),
        url::Origin::Create(app_identifier.origin()),
        content::DevToolsBackgroundService::kPushMessaging,
        "Generic notification shown" /* event_name */, push_message_id,
        {} /* event_metadata */);
  }
}

// Other gcm::GCMAppHandler methods --------------------------------------------

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {
  // TODO(mvanouwerkerk): Consider firing an event on the Service Worker
  // corresponding to |app_id| to inform the app about deleted messages.
}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnMessageDecryptionFailed(
    const std::string& app_id,
    const std::string& message_id,
    const std::string& error_message) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(pref_service_, app_id);

  if (app_identifier.is_null())
    return;

  LogMessageReceivedEventToDevTools(
      GetDevToolsContext(app_identifier.origin()), app_identifier, message_id,
      /* was_encrypted= */ true, error_message, "" /* payload */);
}

// Subscribe and GetPermissionStatus methods -----------------------------------

void PushMessagingServiceImpl::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    int render_frame_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    RegisterCallback callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service_, requesting_origin, service_worker_registration_id);

  LOG(INFO) << __func__
            << " app_identifier.is_null()=" << app_identifier.is_null();

  // If there is no existing app identifier for the given Service Worker,
  // generate a new one. This will create a new subscription on the server.
  if (app_identifier.is_null()) {
    app_identifier = PushMessagingAppIdentifier::Generate(
        requesting_origin, service_worker_registration_id);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(std::move(callback),
                          blink::mojom::PushRegistrationStatus::LIMIT_REACHED);
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  if (!render_frame_host) {
    // It is possible for `render_frame_host` to be nullptr here due to a race
    // (crbug.com/1057981).
    SubscribeEndWithError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::RENDERER_SHUTDOWN);
    return;
  }

  if (!options->user_visible_only) {
    content::RenderFrameHost* main_frame =
        GetMainFrameForRenderFrameHost(render_frame_host);

    if (main_frame) {
      main_frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                                      kSilentPushUnsupportedMessage);
    }

    SubscribeEndWithError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  // Push does not allow permission requests from iframes.
  browser_context_->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          blink::PermissionType::NOTIFICATIONS, render_frame_host, user_gesture,
          base::BindOnce(&PushMessagingServiceImpl::DoSubscribe,
                         weak_factory_.GetWeakPtr(), std::move(app_identifier),
                         std::move(options), std::move(callback),
                         render_process_id, render_frame_id));
}

void PushMessagingServiceImpl::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback register_callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service_, requesting_origin, service_worker_registration_id);

  // If there is no existing app identifier for the given Service Worker,
  // generate a new one. This will create a new subscription on the server.
  if (app_identifier.is_null()) {
    app_identifier = PushMessagingAppIdentifier::Generate(
        requesting_origin, service_worker_registration_id);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(std::move(register_callback),
                          blink::mojom::PushRegistrationStatus::LIMIT_REACHED);
    return;
  }

  if (!IsPermissionSet(requesting_origin, options->user_visible_only)) {
    SubscribeEndWithError(
        std::move(register_callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  DoSubscribe(std::move(app_identifier), std::move(options),
              std::move(register_callback),
              /* render_process_id= */ -1, /* render_frame_id= */ -1,
              blink::mojom::PermissionStatus::GRANTED);
}

blink::mojom::PermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& origin,
    bool user_visible) {
  if (!user_visible)
    return blink::mojom::PermissionStatus::DENIED;

  // Because the Push API is tied to Service Workers, many usages of the API
  // won't have an embedding origin at all. Only consider the requesting
  // |origin| when checking whether permission to use the API has been
  // granted.
  return browser_context_->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
      .status;
}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::DoSubscribe(
    PushMessagingAppIdentifier app_identifier,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback register_callback,
    int render_process_id,
    int render_frame_id,
    blink::mojom::PermissionStatus content_setting) {
  if (content_setting != blink::mojom::PermissionStatus::GRANTED) {
    SubscribeEndWithError(
        std::move(register_callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  std::string application_server_key_string(
      options->application_server_key.begin(),
      options->application_server_key.end());

  // TODO(peter): Move this check to the renderer process & Mojo message
  // validation once the flag is always enabled, and remove the
  // |render_process_id| and |render_frame_id| parameters from this method.
  if (!IsVapidKey(application_server_key_string)) {
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    content::RenderFrameHost* main_frame =
        GetMainFrameForRenderFrameHost(render_frame_host);

    if (base::FeatureList::IsEnabled(
            features::kPushMessagingDisallowSenderIDs)) {
      if (main_frame) {
        main_frame->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            kSenderIdRegistrationDisallowedMessage);
      }
      SubscribeEndWithError(
          std::move(register_callback),
          blink::mojom::PushRegistrationStatus::UNSUPPORTED_GCM_SENDER_ID);
      return;
    } else if (main_frame) {
      main_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          kSenderIdRegistrationDeprecatedMessage);
    }
  }

  IncreasePushSubscriptionCount(1, true /* is_pending */);

  // Set time to live for GCM registration
  base::TimeDelta ttl = base::TimeDelta();

  if (base::FeatureList::IsEnabled(
          features::kPushSubscriptionWithExpirationTime)) {
    app_identifier.set_expiration_time(
        base::Time::Now() + kPushSubscriptionExpirationPeriodTimeDelta);
    DCHECK(app_identifier.expiration_time());
    ttl = kPushSubscriptionExpirationPeriodTimeDelta;
  }

  GetInstanceIDDriver()
      ->GetInstanceID(app_identifier.app_id())
      ->GetToken(NormalizeSenderInfo(application_server_key_string), kGCMScope,
                 ttl, {} /* flags */,
                 base::BindOnce(&PushMessagingServiceImpl::DidSubscribe,
                                weak_factory_.GetWeakPtr(), app_identifier,
                                application_server_key_string,
                                std::move(register_callback)));
}

void PushMessagingServiceImpl::SubscribeEnd(
    RegisterCallback callback,
    const std::string& subscription_id,
    const GURL& endpoint,
    const absl::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    blink::mojom::PushRegistrationStatus status) {
  std::move(callback).Run(subscription_id, endpoint, expiration_time, p256dh,
                          auth, status);
}

void PushMessagingServiceImpl::SubscribeEndWithError(
    RegisterCallback callback,
    blink::mojom::PushRegistrationStatus status) {
  SubscribeEnd(std::move(callback), std::string() /* subscription_id */,
               GURL::EmptyGURL() /* endpoint */,
               absl::nullopt /* expiration_time */,
               std::vector<uint8_t>() /* p256dh */,
               std::vector<uint8_t>() /* auth */, status);
}

void PushMessagingServiceImpl::DidSubscribe(
    const PushMessagingAppIdentifier& app_identifier,
    const std::string& sender_id,
    RegisterCallback callback,
    const std::string& subscription_id,
    instance_id::InstanceID::Result result) {
  DecreasePushSubscriptionCount(1, true /* was_pending */);

  blink::mojom::PushRegistrationStatus status =
      blink::mojom::PushRegistrationStatus::SERVICE_ERROR;

  switch (result) {
    case instance_id::InstanceID::SUCCESS: {
      const GURL endpoint = CreateEndpoint(subscription_id);

      // Make sure that this subscription has associated encryption keys prior
      // to returning it to the developer - they'll need this information in
      // order to send payloads to the user.
      GetEncryptionInfoForAppId(
          app_identifier.app_id(), sender_id,
          base::BindOnce(
              &PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo,
              weak_factory_.GetWeakPtr(), app_identifier, std::move(callback),
              subscription_id, endpoint));
      return;
    }
    case instance_id::InstanceID::INVALID_PARAMETER:
    case instance_id::InstanceID::DISABLED:
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
    case instance_id::InstanceID::SERVER_ERROR:
    case instance_id::InstanceID::UNKNOWN_ERROR:
      DLOG(ERROR) << "Push messaging subscription failed; "
                     "instance_id::InstanceID::Result = "
                  << result;
      status = blink::mojom::PushRegistrationStatus::SERVICE_ERROR;
      break;
    case instance_id::InstanceID::NETWORK_ERROR:
      status = blink::mojom::PushRegistrationStatus::NETWORK_ERROR;
      break;
  }

  SubscribeEndWithError(std::move(callback), status);
}

void PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo(
    const PushMessagingAppIdentifier& app_identifier,
    RegisterCallback callback,
    const std::string& subscription_id,
    const GURL& endpoint,
    std::string p256dh,
    std::string auth_secret) {
  if (p256dh.empty()) {
    SubscribeEndWithError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE);
    return;
  }

  app_identifier.PersistToPrefs(pref_service_);

  IncreasePushSubscriptionCount(1, false /* is_pending */);

  SubscribeEnd(std::move(callback), subscription_id, endpoint,
               app_identifier.expiration_time(),
               std::vector<uint8_t>(p256dh.begin(), p256dh.end()),
               std::vector<uint8_t>(auth_secret.begin(), auth_secret.end()),
               blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE);
}

// GetSubscriptionInfo methods -------------------------------------------------

void PushMessagingServiceImpl::GetSubscriptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& subscription_id,
    SubscriptionInfoCallback callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service_, origin, service_worker_registration_id);

  if (app_identifier.is_null()) {
    std::move(callback).Run(
        false /* is_valid */, GURL::EmptyGURL() /*endpoint*/,
        absl::nullopt /* expiration_time */,
        std::vector<uint8_t>() /* p256dh */, std::vector<uint8_t>() /* auth */);
    return;
  }

  const GURL endpoint = CreateEndpoint(subscription_id);
  const std::string& app_id = app_identifier.app_id();
  absl::optional<base::Time> expiration_time = app_identifier.expiration_time();

  base::OnceCallback<void(bool)> validate_cb =
      base::BindOnce(&PushMessagingServiceImpl::DidValidateSubscription,
                     weak_factory_.GetWeakPtr(), app_id, sender_id, endpoint,
                     expiration_time, std::move(callback));

  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->ValidateToken(
        NormalizeSenderInfo(sender_id), kGCMScope, subscription_id,
        std::move(validate_cb));
  } else {
    GetGCMDriver()->ValidateRegistration(
        app_id, {NormalizeSenderInfo(sender_id)}, subscription_id,
        std::move(validate_cb));
  }
}

void PushMessagingServiceImpl::DidValidateSubscription(
    const std::string& app_id,
    const std::string& sender_id,
    const GURL& endpoint,
    const absl::optional<base::Time>& expiration_time,
    SubscriptionInfoCallback callback,
    bool is_valid) {
  if (!is_valid) {
    std::move(callback).Run(
        false /* is_valid */, GURL::EmptyGURL() /* endpoint */,
        absl::nullopt /* expiration_time */,
        std::vector<uint8_t>() /* p256dh */, std::vector<uint8_t>() /* auth */);
    return;
  }

  GetEncryptionInfoForAppId(
      app_id, sender_id,
      base::BindOnce(&PushMessagingServiceImpl::DidGetEncryptionInfo,
                     weak_factory_.GetWeakPtr(), endpoint, expiration_time,
                     std::move(callback)));
}

void PushMessagingServiceImpl::DidGetEncryptionInfo(
    const GURL& endpoint,
    const absl::optional<base::Time>& expiration_time,
    SubscriptionInfoCallback callback,
    std::string p256dh,
    std::string auth_secret) const {
  // I/O errors might prevent the GCM Driver from retrieving a key-pair.
  bool is_valid = !p256dh.empty();
  std::move(callback).Run(
      is_valid, endpoint, expiration_time,
      std::vector<uint8_t>(p256dh.begin(), p256dh.end()),
      std::vector<uint8_t>(auth_secret.begin(), auth_secret.end()));
}

// Unsubscribe methods ---------------------------------------------------------

void PushMessagingServiceImpl::Unsubscribe(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service_, requesting_origin, service_worker_registration_id);

  UnsubscribeInternal(
      reason, requesting_origin, service_worker_registration_id,
      app_identifier.is_null() ? std::string() : app_identifier.app_id(),
      sender_id, std::move(callback));
}

void PushMessagingServiceImpl::UnsubscribeInternal(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& app_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  DCHECK(!app_id.empty() || (!origin.is_empty() &&
                             service_worker_registration_id !=
                                 -1 /* kInvalidServiceWorkerRegistrationId */))
      << "Need an app_id and/or origin+service_worker_registration_id";

  if (origin.is_empty() || service_worker_registration_id ==
                               -1 /* kInvalidServiceWorkerRegistrationId */) {
    // Can't clear Service Worker database.
    DidClearPushSubscriptionId(reason, app_id, sender_id, std::move(callback));
    return;
  }
  ClearPushSubscriptionId(
      browser_context_, origin, service_worker_registration_id,
      base::BindOnce(&PushMessagingServiceImpl::DidClearPushSubscriptionId,
                     weak_factory_.GetWeakPtr(), reason, app_id, sender_id,
                     std::move(callback)));
}

void PushMessagingServiceImpl::DidClearPushSubscriptionId(
    blink::mojom::PushUnregistrationReason reason,
    const std::string& app_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  if (app_id.empty()) {
    // Without an |app_id|, we can neither delete the subscription from the
    // PushMessagingAppIdentifier map, nor unsubscribe with the GCM Driver.
    std::move(callback).Run(
        blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED);
    return;
  }

  // Delete the mapping for this app_id, to guarantee that no messages get
  // delivered in future (even if unregistration fails).
  // TODO(johnme): Instead of deleting these app ids, store them elsewhere, and
  // retry unregistration if it fails due to network errors (crbug.com/465399).
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(pref_service_, app_id);
  bool was_subscribed = !app_identifier.is_null();
  if (was_subscribed)
    app_identifier.DeleteFromPrefs(pref_service_);

  // Run the unsubscribe callback *before* asking the InstanceIDDriver/GCMDriver
  // to unsubscribe, since that's a slow process involving network retries, and
  // by this point enough local state has been deleted that the subscription is
  // inactive. Note that DeliverMessageCallback automatically unsubscribes if
  // messages are later received for a subscription that was locally deleted,
  // so as long as messages keep getting sent to it, the unsubscription should
  // eventually reach GCM servers even if this particular attempt fails.
  std::move(callback).Run(
      was_subscribed
          ? blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED
          : blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED);

  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->DeleteID(
        base::BindOnce(&PushMessagingServiceImpl::DidDeleteID,
                       weak_factory_.GetWeakPtr(), app_id, was_subscribed));

  } else {
    auto unregister_callback =
        base::BindOnce(&PushMessagingServiceImpl::DidUnregister,
                       weak_factory_.GetWeakPtr(), was_subscribed);
#if defined(OS_ANDROID)
    // On Android the backend is different, and requires the original sender_id.
    // DidGetSenderIdUnexpectedUnsubscribe and
    // DidDeleteServiceWorkerRegistration sometimes call us with an empty one.
    if (sender_id.empty()) {
      std::move(unregister_callback).Run(gcm::GCMClient::INVALID_PARAMETER);
    } else {
      GetGCMDriver()->UnregisterWithSenderId(app_id,
                                             NormalizeSenderInfo(sender_id),
                                             std::move(unregister_callback));
    }
#else
    GetGCMDriver()->Unregister(app_id, std::move(unregister_callback));
#endif
  }
}

void PushMessagingServiceImpl::DidUnregister(bool was_subscribed,
                                             gcm::GCMClient::Result result) {
  DidUnsubscribe(std::string() /* app_id_when_instance_id */, was_subscribed);
}

void PushMessagingServiceImpl::DidDeleteID(
    const std::string& app_id,
    bool was_subscribed,
    instance_id::InstanceID::Result result) {
  // DidUnsubscribe must be run asynchronously when passing a non-empty
  // |app_id_when_instance_id|, since it calls
  // InstanceIDDriver::RemoveInstanceID which deletes the InstanceID itself.
  // Calling that immediately would cause a use-after-free in our caller.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PushMessagingServiceImpl::DidUnsubscribe,
                     weak_factory_.GetWeakPtr(), app_id, was_subscribed));
}

void PushMessagingServiceImpl::DidUnsubscribe(
    const std::string& app_id_when_instance_id,
    bool was_subscribed) {
  if (!app_id_when_instance_id.empty())
    GetInstanceIDDriver()->RemoveInstanceID(app_id_when_instance_id);

  if (was_subscribed)
    DecreasePushSubscriptionCount(1, false /* was_pending */);
}

// DidDeleteServiceWorkerRegistration methods ----------------------------------

void PushMessagingServiceImpl::DidDeleteServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  const PushMessagingAppIdentifier& app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service_, origin, service_worker_registration_id);
  if (app_identifier.is_null()) {
    return;
  }

  base::RepeatingClosure completed_closure = base::DoNothing();
  // Note this will not fully unsubscribe pre-InstanceID subscriptions on
  // Android from GCM, as that requires a sender_id. (Ideally we'd fetch it
  // from the SWDB in some "before_unregistered" SWObserver event.)
  UnsubscribeInternal(
      blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED,
      origin, service_worker_registration_id, app_identifier.app_id(),
      std::string() /* sender_id */,
      base::BindOnce(&UnregisterCallbackToClosure, completed_closure));
}

// DidDeleteServiceWorkerDatabase methods --------------------------------------

void PushMessagingServiceImpl::DidDeleteServiceWorkerDatabase() {
  std::vector<PushMessagingAppIdentifier> app_identifiers =
      PushMessagingAppIdentifier::GetAll(pref_service_);

  base::RepeatingClosure completed_closure =
      base::BarrierClosure(app_identifiers.size(), base::DoNothing());

  for (const PushMessagingAppIdentifier& app_identifier : app_identifiers) {
    // Note this will not fully unsubscribe pre-InstanceID subscriptions on
    // Android from GCM, as that requires a sender_id. We can't fetch those from
    // the Service Worker database anymore as it's been deleted.
    UnsubscribeInternal(
        blink::mojom::PushUnregistrationReason::SERVICE_WORKER_DATABASE_WIPED,
        app_identifier.origin(),
        app_identifier.service_worker_registration_id(),
        app_identifier.app_id(), std::string() /* sender_id */,
        base::BindOnce(&UnregisterCallbackToClosure, completed_closure));
  }
}

void PushMessagingServiceImpl::UnexpectedUnsubscribe(
    const PushMessagingAppIdentifier& app_identifier,
    blink::mojom::PushUnregistrationReason reason,
    UnregisterCallback unregister_callback) {
  UnsubscribeInternal(reason, app_identifier.origin(),
                      app_identifier.service_worker_registration_id(),
                      app_identifier.app_id(), std::string() /* sender_id */,
                      std::move(unregister_callback));
}

void PushMessagingServiceImpl::GetPushSubscriptionFromAppIdentifier(
    const PushMessagingAppIdentifier& app_identifier,
    base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)>
        subscription_cb) {
  GetSWData(browser_context_, app_identifier.origin(),
            app_identifier.service_worker_registration_id(),
            base::BindOnce(&PushMessagingServiceImpl::DidGetSWData,
                           weak_factory_.GetWeakPtr(), app_identifier,
                           std::move(subscription_cb)));
}

void PushMessagingServiceImpl::DidGetSWData(
    const PushMessagingAppIdentifier& app_identifier,
    base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> subscription_cb,
    const std::string& sender_id,
    const std::string& subscription_id) {
  // SW Database was corrupted, return immediately
  if (sender_id.empty() || subscription_id.empty()) {
    std::move(subscription_cb).Run(nullptr /* subscription */);
    return;
  }
  GetSubscriptionInfo(
      app_identifier.origin(), app_identifier.service_worker_registration_id(),
      sender_id, subscription_id,
      base::BindOnce(
          &PushMessagingServiceImpl::GetPushSubscriptionFromAppIdentifierEnd,
          weak_factory_.GetWeakPtr(), std::move(subscription_cb), sender_id));
}

void PushMessagingServiceImpl::GetPushSubscriptionFromAppIdentifierEnd(
    base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback,
    const std::string& sender_id,
    bool is_valid,
    const GURL& endpoint,
    const absl::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  if (!is_valid) {
    // TODO(viviy): Log error in UMA
    std::move(callback).Run(nullptr /* subscription */);
    return;
  }

  std::move(callback).Run(blink::mojom::PushSubscription::New(
      endpoint, expiration_time, push_messaging::MakeOptions(sender_id), p256dh,
      auth));
}

void PushMessagingServiceImpl::FirePushSubscriptionChange(
    const PushMessagingAppIdentifier& app_identifier,
    base::OnceClosure completed_closure,
    blink::mojom::PushSubscriptionPtr new_subscription,
    blink::mojom::PushSubscriptionPtr old_subscription) {
  // Ensure |completed_closure| is run after this function
  base::ScopedClosureRunner scoped_closure(std::move(completed_closure));

  if (!base::FeatureList::IsEnabled(features::kPushSubscriptionChangeEvent))
    return;

  if (app_identifier.is_null()) {
    FirePushSubscriptionChangeCallback(
        app_identifier, blink::mojom::PushEventStatus::UNKNOWN_APP_ID);
    return;
  }

  browser_context_->FirePushSubscriptionChangeEvent(
      app_identifier.origin(), app_identifier.service_worker_registration_id(),
      std::move(new_subscription), std::move(old_subscription),
      base::BindOnce(
          &PushMessagingServiceImpl::FirePushSubscriptionChangeCallback,
          weak_factory_.GetWeakPtr(), app_identifier));
}

void PushMessagingServiceImpl::FirePushSubscriptionChangeCallback(
    const PushMessagingAppIdentifier& app_identifier,
    blink::mojom::PushEventStatus status) {}

// KeyedService methods -------------------------------------------------------
void PushMessagingServiceImpl::Shutdown() {
  GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
}

// Helper methods --------------------------------------------------------------
// Assumes user_visible always since this is just meant to check
// if the permission was previously granted and not revoked.
bool PushMessagingServiceImpl::IsPermissionSet(const GURL& origin,
                                               bool user_visible) {
  return GetPermissionStatus(origin, user_visible) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void PushMessagingServiceImpl::GetEncryptionInfoForAppId(
    const std::string& app_id,
    const std::string& sender_id,
    gcm::GCMEncryptionProvider::EncryptionInfoCallback callback) {
  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->GetEncryptionInfo(
        NormalizeSenderInfo(sender_id), std::move(callback));
  } else {
    GetGCMDriver()->GetEncryptionInfo(app_id, std::move(callback));
  }
}

gcm::GCMDriver* PushMessagingServiceImpl::GetGCMDriver() const {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(browser_context_);
  CHECK(gcm_profile_service);
  CHECK(gcm_profile_service->driver());
  return gcm_profile_service->driver();
}

instance_id::InstanceIDDriver* PushMessagingServiceImpl::GetInstanceIDDriver()
    const {
  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(
          browser_context_);
  CHECK(instance_id_profile_service);
  CHECK(instance_id_profile_service->driver());
  return instance_id_profile_service->driver();
}

content::DevToolsBackgroundServicesContext*
PushMessagingServiceImpl::GetDevToolsContext(const GURL& origin) const {
  auto* storage_partition = browser_context_->GetStoragePartitionForUrl(origin);
  if (!storage_partition)
    return nullptr;

  auto* devtools_context =
      storage_partition->GetDevToolsBackgroundServicesContext();

  if (!devtools_context->IsRecording(
          content::DevToolsBackgroundService::kPushMessaging)) {
    return nullptr;
  }

  return devtools_context;
}
}  // namespace neva_app_runtime
