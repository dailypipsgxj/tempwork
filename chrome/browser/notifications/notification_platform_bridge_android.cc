// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/native_notification_display_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "jni/NotificationPlatformBridge_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

ScopedJavaLocalRef<jobjectArray> ConvertToJavaBitmaps(
    const std::vector<message_center::ButtonInfo>& buttons) {
  std::vector<SkBitmap> skbitmaps;
  for (const message_center::ButtonInfo& button : buttons)
    skbitmaps.push_back(button.icon.AsBitmap());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "android/graphics/Bitmap");
  jobjectArray array = env->NewObjectArray(skbitmaps.size(), clazz.obj(),
                                           nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < skbitmaps.size(); ++i) {
    if (!skbitmaps[i].drawsNothing()) {
      env->SetObjectArrayElement(
          array, i, gfx::ConvertToJavaBitmap(&(skbitmaps[i])).obj());
    }
  }

  return ScopedJavaLocalRef<jobjectArray>(env, array);
}

// Callback to run once the profile has been loaded in order to perform a
// given |operation| in a notification.
// TODO(miguelg) move it to notification_common?
void ProfileLoadedCallback(NotificationCommon::Operation operation,
                           NotificationCommon::Type notification_type,
                           const std::string& origin,
                           const std::string& notification_id,
                           int action_index,
                           Profile* profile) {
  if (!profile) {
    // TODO(miguelg): Add UMA for this condition.
    // Perhaps propagate this through PersistentNotificationStatus.
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }

  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  static_cast<NativeNotificationDisplayService*>(display_service)
      ->ProcessNotificationOperation(operation, notification_type, origin,
                                     notification_id, action_index);
}

}  // namespace

// Called by the Java side when a notification event has been received, but the
// NotificationBridge has not been initialized yet. Enforce initialization of
// the class.
static void InitializeNotificationPlatformBridge(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  g_browser_process->notification_platform_bridge();
}

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeAndroid();
}

NotificationPlatformBridgeAndroid::NotificationPlatformBridgeAndroid() {
  java_object_.Reset(Java_NotificationPlatformBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

NotificationPlatformBridgeAndroid::~NotificationPlatformBridgeAndroid() {
  Java_NotificationPlatformBridge_destroy(AttachCurrentThread(),
                                          java_object_.obj());
}

void NotificationPlatformBridgeAndroid::OnNotificationClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jstring>& java_notification_id,
    const JavaParamRef<jstring>& java_origin,
    const JavaParamRef<jstring>& java_profile_id,
    jboolean incognito,
    const JavaParamRef<jstring>& java_tag,
    const JavaParamRef<jstring>& java_webapk_package,
    jint action_index) {
  std::string notification_id =
      ConvertJavaStringToUTF8(env, java_notification_id);
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);
  std::string profile_id = ConvertJavaStringToUTF8(env, java_profile_id);
  std::string webapk_package =
      ConvertJavaStringToUTF8(env, java_webapk_package);

  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  regenerated_notification_infos_[notification_id] =
      RegeneratedNotificationInfo(origin.spec(), tag, webapk_package);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  profile_manager->LoadProfile(
      profile_id, incognito,
      base::Bind(&ProfileLoadedCallback, NotificationCommon::CLICK,
                 NotificationCommon::PERSISTENT, origin.spec(), notification_id,
                 action_index));
}

void NotificationPlatformBridgeAndroid::OnNotificationClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jstring>& java_notification_id,
    const JavaParamRef<jstring>& java_origin,
    const JavaParamRef<jstring>& java_profile_id,
    jboolean incognito,
    const JavaParamRef<jstring>& java_tag,
    jboolean by_user) {
  std::string profile_id = ConvertJavaStringToUTF8(env, java_profile_id);
  std::string notification_id =
      ConvertJavaStringToUTF8(env, java_notification_id);

  // The notification was closed by the platform, so clear all local state.
  regenerated_notification_infos_.erase(notification_id);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  profile_manager->LoadProfile(
      profile_id, incognito,
      base::Bind(&ProfileLoadedCallback, NotificationCommon::CLOSE,
                 NotificationCommon::PERSISTENT,
                 ConvertJavaStringToUTF8(env, java_origin), notification_id,
                 -1 /* action index */));
}

void NotificationPlatformBridgeAndroid::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const Notification& notification) {
  JNIEnv* env = AttachCurrentThread();
  // TODO(miguelg): Store the notification type in java instead of assuming it's
  // persistent once/if non persistent notifications are ever implemented on
  // Android.
  DCHECK_EQ(notification_type, NotificationCommon::PERSISTENT);

  GURL origin_url(notification.origin_url().GetOrigin());
  ScopedJavaLocalRef<jstring> webapk_package;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebApk)) {
    GURL scope_url(notification.service_worker_scope());
    if (!scope_url.is_valid())
      scope_url = origin_url;
    ScopedJavaLocalRef<jstring> j_scope_url =
        ConvertUTF8ToJavaString(env, scope_url.spec());
    webapk_package = Java_NotificationPlatformBridge_queryWebApkPackage(
        env, java_object_.obj(), j_scope_url.obj());
  } else {
    webapk_package = ConvertUTF8ToJavaString(env, "");
  }

  ScopedJavaLocalRef<jstring> j_notification_id =
      ConvertUTF8ToJavaString(env, notification_id);
  ScopedJavaLocalRef<jstring> j_origin =
      ConvertUTF8ToJavaString(env, origin_url.spec());
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification.tag());
  ScopedJavaLocalRef<jstring> title =
      ConvertUTF16ToJavaString(env, notification.title());
  ScopedJavaLocalRef<jstring> body =
      ConvertUTF16ToJavaString(env, notification.message());

  ScopedJavaLocalRef<jobject> notification_icon;
  SkBitmap notification_icon_bitmap = notification.icon().AsBitmap();
  if (!notification_icon_bitmap.drawsNothing())
    notification_icon = gfx::ConvertToJavaBitmap(&notification_icon_bitmap);

  ScopedJavaLocalRef<jobject> badge;
  SkBitmap badge_bitmap = notification.small_image().AsBitmap();
  if (!badge_bitmap.drawsNothing())
    badge = gfx::ConvertToJavaBitmap(&badge_bitmap);

  std::vector<base::string16> action_titles_vector;
  for (const message_center::ButtonInfo& button : notification.buttons())
    action_titles_vector.push_back(button.title);
  ScopedJavaLocalRef<jobjectArray> action_titles =
      base::android::ToJavaArrayOfStrings(env, action_titles_vector);

  ScopedJavaLocalRef<jobjectArray> action_icons =
      ConvertToJavaBitmaps(notification.buttons());

  ScopedJavaLocalRef<jintArray> vibration_pattern =
      base::android::ToJavaIntArray(env, notification.vibration_pattern());

  ScopedJavaLocalRef<jstring> j_profile_id =
      ConvertUTF8ToJavaString(env, profile_id);

  Java_NotificationPlatformBridge_displayNotification(
      env, java_object_.obj(), j_notification_id.obj(), j_origin.obj(),
      j_profile_id.obj(), incognito, tag.obj(), webapk_package.obj(),
      title.obj(), body.obj(), notification_icon.obj(), badge.obj(),
      vibration_pattern.obj(), notification.timestamp().ToJavaTime(),
      notification.renotify(), notification.silent(), action_titles.obj(),
      action_icons.obj());

  regenerated_notification_infos_[notification_id] =
      RegeneratedNotificationInfo(origin_url.spec(), notification.tag(),
                                  ConvertJavaStringToUTF8(env, webapk_package));
}

void NotificationPlatformBridgeAndroid::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  const auto iterator = regenerated_notification_infos_.find(notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  const RegeneratedNotificationInfo& notification_info = iterator->second;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_notification_id =
      ConvertUTF8ToJavaString(env, notification_id);
  ScopedJavaLocalRef<jstring> origin =
      ConvertUTF8ToJavaString(env, notification_info.origin);
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification_info.tag);
  ScopedJavaLocalRef<jstring> webapk_package =
      ConvertUTF8ToJavaString(env, notification_info.webapk_package);

  ScopedJavaLocalRef<jstring> j_profile_id =
      ConvertUTF8ToJavaString(env, profile_id);

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationPlatformBridge_closeNotification(
      env, java_object_.obj(), j_profile_id.obj(), j_notification_id.obj(),
      origin.obj(), tag.obj(), webapk_package.obj());
}

bool NotificationPlatformBridgeAndroid::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    std::set<std::string>* notifications) const {
  // TODO(miguelg): This can actually be implemented for M+
  return false;
}

bool NotificationPlatformBridgeAndroid::SupportsNotificationCenter() const {
  return true;
}

// static
bool NotificationPlatformBridgeAndroid::RegisterNotificationPlatformBridge(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void NotificationPlatformBridgeAndroid::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNotificationsVibrateEnabled, true);
}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo() {}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    RegeneratedNotificationInfo(const std::string& origin,
                                const std::string& tag,
                                const std::string& webapk_package)
    : origin(origin), tag(tag), webapk_package(webapk_package) {}

NotificationPlatformBridgeAndroid::RegeneratedNotificationInfo::
    ~RegeneratedNotificationInfo() {}
