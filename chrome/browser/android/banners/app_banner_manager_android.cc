// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerAndroid);

namespace {

const char kPlayPlatform[] = "play";
const char kReferrerName[] = "referrer";
const char kIdName[] = "id";
const char kPlayInlineReferrer[] = "playinline=chrome_inline";

}  // anonymous namespace

namespace banners {

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
  CreateJavaBannerManager();
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_.obj());
  java_banner_manager_.Reset();
}

const base::android::ScopedJavaGlobalRef<jobject>&
AppBannerManagerAndroid::GetJavaBannerManager() const {
  return java_banner_manager_;
}

bool AppBannerManagerAndroid::IsFetcherActive(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return AppBannerManager::IsFetcherActive();
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  AppBannerDataFetcherAndroid* android_fetcher =
      static_cast<AppBannerDataFetcherAndroid*>(data_fetcher().get());
  if (!CheckFetcherMatchesContents(android_fetcher->is_debug_mode()))
    return false;

  GURL image_url = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  return android_fetcher->ContinueFetching(
      ConvertJavaStringToUTF16(env, japp_title),
      ConvertJavaStringToUTF8(env, japp_package), japp_data, image_url);
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url,
                                               bool is_debug_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isEnabledForTab(env, java_banner_manager_.obj()))
    return;

  AppBannerManager::RequestAppBanner(validated_url, is_debug_mode);
}

AppBannerDataFetcher* AppBannerManagerAndroid::CreateAppBannerDataFetcher(
    base::WeakPtr<Delegate> weak_delegate,
    bool is_debug_mode) {
  return new AppBannerDataFetcherAndroid(
      web_contents(), weak_delegate,
      ShortcutHelper::GetIdealHomescreenIconSizeInDp(),
      ShortcutHelper::GetMinimumHomescreenIconSizeInDp(),
      ShortcutHelper::GetIdealSplashImageSizeInDp(),
      ShortcutHelper::GetMinimumSplashImageSizeInDp(), is_debug_mode);
}

bool AppBannerManagerAndroid::HandleNonWebApp(const std::string& platform,
                                              const GURL& url,
                                              const std::string& id,
                                              bool is_debug_mode) {
  if (!CheckPlatformAndId(platform, id, is_debug_mode))
    return false;

  banners::TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_banner_manager_.is_null())
    return false;

  std::string id_from_app_url = ExtractQueryValueForName(url, kIdName);
  if (id_from_app_url.size() && id != id_from_app_url) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kIgnoredIdsDoNotMatch, is_debug_mode);
    return false;
  }

  std::string referrer =
      ExtractQueryValueForName(url, kReferrerName);

  // Attach the chrome_inline referrer value, prefixed with "&" if the referrer
  // is non empty.
  if (referrer.empty())
    referrer = kPlayInlineReferrer;
  else
    referrer.append("&").append(kPlayInlineReferrer);

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, data_fetcher()->validated_url().spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));
  Java_AppBannerManager_fetchAppDetails(
      env, java_banner_manager_.obj(), jurl.obj(),
      jpackage.obj(), jreferrer.obj(),
      ShortcutHelper::GetIdealHomescreenIconSizeInDp());
  return true;
}

void AppBannerManagerAndroid::CreateJavaBannerManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_banner_manager_.Reset(
      Java_AppBannerManager_create(env, reinterpret_cast<intptr_t>(this)));
}

bool AppBannerManagerAndroid::CheckFetcherMatchesContents(bool is_debug_mode) {
  if (!web_contents())
    return false;

  if (!data_fetcher() ||
      data_fetcher()->validated_url() != web_contents()->GetURL()) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kUserNavigatedBeforeBannerShown,
        is_debug_mode);
    return false;
  }
  return true;
}

bool AppBannerManagerAndroid::CheckPlatformAndId(const std::string& platform,
                                                 const std::string& id,
                                                 bool is_debug_mode) {
  if (platform != kPlayPlatform) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kIgnoredNotSupportedOnAndroid, platform,
        is_debug_mode);
    return false;
  }
  if (id.empty()) {
    banners::OutputDeveloperNotShownMessage(
        web_contents(), banners::kIgnoredNoId, is_debug_mode);
    return false;
  }
  return true;
}

std::string AppBannerManagerAndroid::ExtractQueryValueForName(
    const GURL& url,
    const std::string& name) {
  url::Component query = url.parsed_for_possibly_invalid_spec().query;
  url::Component key, value;
  const char* url_spec = url.spec().c_str();

  while (url::ExtractQueryKeyValue(url_spec, &query, &key, &value)) {
    std::string key_str(url_spec, key.begin, key.len);
    std::string value_str(url_spec, value.begin, value.len);
    if (key_str == name)
      return value_str;
  }
  return "";
}

// static
bool AppBannerManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
ScopedJavaLocalRef<jobject> GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  AppBannerManagerAndroid* manager = AppBannerManagerAndroid::FromWebContents(
      content::WebContents::FromJavaWebContents(java_web_contents));
  if (!manager)
    return ScopedJavaLocalRef<jobject>();

  return ScopedJavaLocalRef<jobject>(manager->GetJavaBannerManager());
}

// static
void DisableSecureSchemeCheckForTesting(JNIEnv* env,
                                        const JavaParamRef<jclass>& clazz) {
  AppBannerManager::DisableSecureSchemeCheckForTesting();
}

// static
void SetEngagementWeights(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          jdouble direct_engagement,
                          jdouble indirect_engagement) {
  AppBannerManager::SetEngagementWeights(direct_engagement,
                                         indirect_engagement);
}

// static
void SetTimeDeltaForTesting(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            jint days) {
  AppBannerDataFetcher::SetTimeDeltaForTesting(days);
}

}  // namespace banners
