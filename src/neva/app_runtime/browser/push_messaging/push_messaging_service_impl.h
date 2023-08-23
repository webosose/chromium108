// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on chrome/browser/push_messaging/push_messaging_service_impl.h
// Rmoved AbusiveOrigin checking becase webOS doesn't use CrowdDeny Component

#ifndef NEVA_APPRUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
#define NEVA_APPRUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_app_identifier.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"

class PrefService;
namespace content {
class BrowserContext;
}

namespace neva_app_runtime {
class PushMessagingServiceImpl : public content::PushMessagingService,
                                 public KeyedService,
                                 public gcm::GCMAppHandler {
 public:
  // If any Service Workers are using push, starts GCM and adds an app handler.
  static void InitializeForProfile(content::BrowserContext* context);

  explicit PushMessagingServiceImpl(content::BrowserContext* browser_context);

  PushMessagingServiceImpl(const PushMessagingServiceImpl&) = delete;
  PushMessagingServiceImpl& operator=(const PushMessagingServiceImpl&) = delete;

  ~PushMessagingServiceImpl() override;

  // Check and remove subscriptions that are expired when |this| is initialized
  void RemoveExpiredSubscriptions();

  // Gets the permission status for the given |origin|.
  blink::mojom::PermissionStatus GetPermissionStatus(const GURL& origin,
                                                     bool user_visible);

  // content::PushMessagingService implementation:
  void SubscribeFromDocument(const GURL& requesting_origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override;
  void SubscribeFromWorker(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           int render_process_id,
                           blink::mojom::PushSubscriptionOptionsPtr options,
                           RegisterCallback callback) override;
  void GetSubscriptionInfo(const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const std::string& subscription_id,
                           SubscriptionInfoCallback callback) override;
  void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                   const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   UnregisterCallback) override;
  bool SupportNonVisibleMessages() override;
  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override;
  void DidDeleteServiceWorkerDatabase() override;

  // Fires the `pushsubscriptionchange` event to the associated service worker
  // of |app_identifier|, which is the app identifier for |old_subscription|
  // whereas |new_subscription| can be either null e.g. when a subscription is
  // lost due to permission changes or a new subscription when it was refreshed.
  void FirePushSubscriptionChange(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceClosure completed_closure,
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription);

  // KeyedService implementation:
  void Shutdown() override;

  // gcm::GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnMessageDecryptionFailed(const std::string& app_id,
                                 const std::string& message_id,
                                 const std::string& error_message) override;
  bool CanHandle(const std::string& app_id) const override;

 private:
  // A subscription is pending until it has succeeded or failed.
  void IncreasePushSubscriptionCount(int add, bool is_pending);
  void DecreasePushSubscriptionCount(int subtract, bool was_pending);

  // OnMessage methods ---------------------------------------------------------
  void DeliverMessageCallback(const std::string& app_id,
                              const GURL& requesting_origin,
                              int64_t service_worker_registration_id,
                              const gcm::IncomingMessage& message,
                              base::OnceClosure message_handled_closure,
                              blink::mojom::PushEventStatus status);

  void DidHandleMessage(const std::string& app_id,
                        const std::string& push_message_id,
                        base::OnceClosure completion_closure,
                        bool did_show_generic_notification);

  base::OnceClosure message_handled_callback() { return base::DoNothing(); }

  // Subscribe methods ---------------------------------------------------------

  void DoSubscribe(PushMessagingAppIdentifier app_identifier,
                   blink::mojom::PushSubscriptionOptionsPtr options,
                   RegisterCallback callback,
                   int render_process_id,
                   int render_frame_id,
                   blink::mojom::PermissionStatus permission_status);

  void SubscribeEnd(RegisterCallback callback,
                    const std::string& subscription_id,
                    const GURL& endpoint,
                    const absl::optional<base::Time>& expiration_time,
                    const std::vector<uint8_t>& p256dh,
                    const std::vector<uint8_t>& auth,
                    blink::mojom::PushRegistrationStatus status);

  void SubscribeEndWithError(RegisterCallback callback,
                             blink::mojom::PushRegistrationStatus status);

  void DidSubscribe(const PushMessagingAppIdentifier& app_identifier,
                    const std::string& sender_id,
                    RegisterCallback callback,
                    const std::string& subscription_id,
                    instance_id::InstanceID::Result result);

  void DidSubscribeWithEncryptionInfo(
      const PushMessagingAppIdentifier& app_identifier,
      RegisterCallback callback,
      const std::string& subscription_id,
      const GURL& endpoint,
      std::string p256dh,
      std::string auth_secret);

  // GetSubscriptionInfo methods -----------------------------------------------

  void DidValidateSubscription(
      const std::string& app_id,
      const std::string& sender_id,
      const GURL& endpoint,
      const absl::optional<base::Time>& expiration_time,
      SubscriptionInfoCallback callback,
      bool is_valid);

  void DidGetEncryptionInfo(const GURL& endpoint,
                            const absl::optional<base::Time>& expiration_time,
                            SubscriptionInfoCallback callback,
                            std::string p256dh,
                            std::string auth_secret) const;

  // Unsubscribe methods -------------------------------------------------------

  // |origin|, |service_worker_registration_id| and |app_id| should be provided
  // whenever they can be obtained. It's valid for |origin| to be empty and
  // |service_worker_registration_id| to be kInvalidServiceWorkerRegistrationId,
  // or for app_id to be empty, but not both at once.
  void UnsubscribeInternal(blink::mojom::PushUnregistrationReason reason,
                           const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& app_id,
                           const std::string& sender_id,
                           UnregisterCallback callback);

  void DidClearPushSubscriptionId(blink::mojom::PushUnregistrationReason reason,
                                  const std::string& app_id,
                                  const std::string& sender_id,
                                  UnregisterCallback callback);

  void DidUnregister(bool was_subscribed, gcm::GCMClient::Result result);
  void DidDeleteID(const std::string& app_id,
                   bool was_subscribed,
                   instance_id::InstanceID::Result result);
  void DidUnsubscribe(const std::string& app_id_when_instance_id,
                      bool was_subscribed);

  void GetPushSubscriptionFromAppIdentifier(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback);

  void DidGetSWData(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback,
      const std::string& sender_id,
      const std::string& subscription_id);

  void GetPushSubscriptionFromAppIdentifierEnd(
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback,
      const std::string& sender_id,
      bool is_valid,
      const GURL& endpoint,
      const absl::optional<base::Time>& expiration_time,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth);
  // Helper methods ------------------------------------------------------------

  // The subscription given in |identifier| will be unsubscribed (and a
  // `pushsubscriptionchange` event fires if
  // features::kPushSubscriptionChangeEvent is enabled)
  void UnexpectedChange(PushMessagingAppIdentifier identifier,
                        blink::mojom::PushUnregistrationReason reason,
                        base::OnceClosure completed_closure);
  void UnexpectedUnsubscribe(const PushMessagingAppIdentifier& app_identifier,
                             blink::mojom::PushUnregistrationReason reason,
                             UnregisterCallback unregister_callback);

  void FirePushSubscriptionChangeCallback(
      const PushMessagingAppIdentifier& app_identifier,
      blink::mojom::PushEventStatus status);

  // Checks if a given origin is allowed to use Push.
  bool IsPermissionSet(const GURL& origin, bool user_visible = true);

  content::DevToolsBackgroundServicesContext* GetDevToolsContext(
      const GURL& origin) const;

  // Wrapper around {GCMDriver, InstanceID}::GetEncryptionInfo.
  void GetEncryptionInfoForAppId(
      const std::string& app_id,
      const std::string& sender_id,
      gcm::GCMEncryptionProvider::EncryptionInfoCallback callback);

  gcm::GCMDriver* GetGCMDriver() const;
  instance_id::InstanceIDDriver* GetInstanceIDDriver() const;

  content::BrowserContext* browser_context_;
  PrefService* pref_service_;
  int push_subscription_count_;
  int pending_push_subscription_count_;

  base::WeakPtrFactory<PushMessagingServiceImpl> weak_factory_{this};
};

}  // namespace neva_app_runtime
#endif  // NEVA_APPRUNTIME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
