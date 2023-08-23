// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on //chrome/browser/notifications/platform_notification_service_impl.cc
// and //chrome/common/pref_names.cc.

#include "neva/app_runtime/browser/notifications/platform_notification_service_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/notifications/notification_display_service.h"
#include "neva/app_runtime/browser/notifications/notification_display_service_factory.h"
#include "neva/app_runtime/browser/push_messaging/push_messaging_app_identifier.h"
#include "ui/message_center/public/cpp/notification.h"

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

  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = service_worker_scope;
  PrefService* pref_service = user_prefs::UserPrefs::Get(context_);
  if (!pref_service) {
    return;
  }
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          pref_service, origin, service_worker_registration_id);
  if (app_identifier.web_app_id().empty()) {
    return;
  }
  metadata->web_app_id = app_identifier.web_app_id();

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

}  // namespace neva_app_runtime
