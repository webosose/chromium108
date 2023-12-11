// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/platform_notification_service_impl.cc
// and //chrome/common/pref_names.cc.

#include "neva/app_runtime/browser/notifications/platform_notification_service_impl.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "neva/app_runtime/browser/notifications/notification_common.h"
#include "neva/app_runtime/browser/notifications/notification_display_service.h"
#include "neva/app_runtime/browser/notifications/notification_display_service_factory.h"
#include "neva/app_runtime/browser/notifications/notification_handler.h"

namespace neva_app_runtime {

namespace {

// Integer that holds the value of the next persistent notification ID to be
// used.
const char kNotificationNextPersistentId[] = "persistent_notifications.next_id";

}  // namespace

// static
void PlatformNotificationServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The first persistent ID is registered as 10000 rather than 1 to prevent the
  // reuse of persistent notification IDs, which must be unique. Reuse of
  // notification IDs may occur as they were previously stored in a different
  // data store.
  registry->RegisterIntegerPref(kNotificationNextPersistentId, 10000);
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl(
    content::BrowserContext* context)
    : context_(context) {}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() = default;

// TODO(awdf): Rename to DisplayNonPersistentNotification (Similar for Close)
void PlatformNotificationServiceImpl::DisplayNotification(
    const std::string& notification_id,
    const GURL& origin,
    const GURL& document_url,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources,
    const int64_t service_worker_registration_id) {
  message_center::Notification notification =
      CreateNotificationFromData(origin, notification_id, notification_data,
                                 notification_resources, service_worker_scope);

  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = service_worker_scope;
  content::StoragePartition* partition =
      context_->GetStoragePartitionForUrl(origin, false);
  if (!partition) {
    LOG(WARNING) << __func__ << ", There is no storage partition";
    return;
  }
  scoped_refptr<content::ServiceWorkerRegistration> registration =
      static_cast<content::ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext())
          ->GetLiveRegistration(service_worker_registration_id);
  if (!registration) {
    LOG(WARNING) << __func__ << ", There is no service worker registration";
    return;
  }
  metadata->web_app_id = registration->scope().get_webapp_id()
                             ? *registration->scope().get_webapp_id()
                             : std::string();

  NotificationDisplayServiceFactory::GetForProfile(context_)->Display(
      NotificationHandler::Type::WEB_PERSISTENT, notification,
      std::move(metadata));
}

void PlatformNotificationServiceImpl::CloseNotification(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    DisplayedNotificationsCallback callback) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::ScheduleTrigger(base::Time timestamp) {
  NOTIMPLEMENTED();
}

base::Time PlatformNotificationServiceImpl::ReadNextTriggerTimestamp() {
  NOTIMPLEMENTED();
  return base::Time::Max();
}

int64_t PlatformNotificationServiceImpl::ReadNextPersistentNotificationId() {
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);

  if (!prefs)
    return 0;

  int64_t current_id = prefs->GetInteger(kNotificationNextPersistentId);
  int64_t next_id = current_id + 1;

  prefs->SetInteger(kNotificationNextPersistentId, next_id);
  return next_id;
}

void PlatformNotificationServiceImpl::RecordNotificationUkmEvent(
    const content::NotificationDatabaseData& data) {
  NOTIMPLEMENTED();
}

message_center::Notification
PlatformNotificationServiceImpl::CreateNotificationFromData(
    const GURL& origin,
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    const blink::NotificationResources& notification_resources,
    const GURL& web_app_hint_url) const {
  // Blink always populates action icons to match the actions, even if no icon
  // was fetched, so this indicates a compromised renderer.
  CHECK_EQ(notification_data.actions.size(),
           notification_resources.action_icons.size());

  message_center::RichNotificationData optional_fields;

  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::INLINE;

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      notification_data.title, notification_data.body,
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(
          notification_resources.notification_icon)),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin), optional_fields,
      nullptr /* delegate */);

  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_timestamp(notification_data.timestamp);
  notification.set_renotify(notification_data.renotify);
  notification.set_silent(notification_data.silent);

  // Developer supplied action buttons.
  std::vector<message_center::ButtonInfo> buttons;
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    const auto& action = notification_data.actions[i];
    message_center::ButtonInfo button(action->title);
    // TODO(peter): Handle different screen densities instead of always using
    // the 1x bitmap - crbug.com/585815.
    button.icon =
        gfx::Image::CreateFrom1xBitmap(notification_resources.action_icons[i]);
    if (action->type == blink::mojom::NotificationActionType::TEXT) {
      button.placeholder = action->placeholder;
    }
    buttons.push_back(button);
  }
  notification.set_buttons(buttons);

  return notification;
}

}  // namespace neva_app_runtime
