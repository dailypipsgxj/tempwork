// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/external_prerender_handler_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/ExternalPrerenderHandler_jni.h"
#include "net/base/network_change_notifier.h"

using base::android::ConvertJavaStringToUTF16;

namespace prerender {

namespace {

bool CheckAndConvertParams(JNIEnv* env,
                           const JavaParamRef<jobject>& jprofile,
                           const JavaParamRef<jstring>& jurl,
                           const JavaParamRef<jobject>& jweb_contents,
                           GURL* url,
                           PrerenderManager** prerender_manager,
                           content::WebContents** web_contents) {
  if (!jurl)
    return false;

  *url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url->is_valid())
    return false;

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  *prerender_manager = PrerenderManagerFactory::GetForProfile(profile);
  if (!*prerender_manager)
    return false;

  *web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  return true;
}

}  // namespace

bool ExternalPrerenderHandlerAndroid::AddPrerender(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jreferrer,
    jint width,
    jint height,
    jboolean prerender_on_cellular) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url.is_valid())
    return false;

  content::Referrer referrer;
  if (!jreferrer.is_null()) {
    GURL referrer_url(ConvertJavaStringToUTF16(env, jreferrer));
    if (referrer_url.is_valid()) {
      referrer =
          content::Referrer(referrer_url, blink::WebReferrerPolicyDefault);
    }
  }

  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return false;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (prerender_handle_)
    prerender_handle_->OnNavigateAway();

  if (prerender_on_cellular && net::NetworkChangeNotifier::IsConnectionCellular(
                   net::NetworkChangeNotifier::GetConnectionType())) {
    prerender_handle_ =
        prerender_manager->AddPrerenderOnCellularFromExternalRequest(
            url, referrer,
            web_contents->GetController().GetDefaultSessionStorageNamespace(),
            gfx::Size(width, height));
  } else {
    prerender_handle_ = prerender_manager->AddPrerenderFromExternalRequest(
        url, referrer,
        web_contents->GetController().GetDefaultSessionStorageNamespace(),
        gfx::Size(width, height));
  }

  return !!prerender_handle_;
}

void ExternalPrerenderHandlerAndroid::CancelCurrentPrerender(
    JNIEnv* env,
    const JavaParamRef<jobject>& object) {
  if (!prerender_handle_)
    return;

  prerender_handle_->OnCancel();
  prerender_handle_.reset();
}

static jboolean HasPrerenderedUrl(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jobject>& jprofile,
                                  const JavaParamRef<jstring>& jurl,
                                  const JavaParamRef<jobject>& jweb_contents) {
  GURL url;
  PrerenderManager* prerender_manager;
  content::WebContents* web_contents;
  if (!CheckAndConvertParams(env, jprofile, jurl, jweb_contents, &url,
                             &prerender_manager, &web_contents))
    return false;

  return prerender_manager->HasPrerenderedUrl(url, web_contents);
}

static jboolean HasPrerenderedAndFinishedLoadingUrl(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jobject>& jweb_contents) {
  GURL url;
  PrerenderManager* prerender_manager;
  content::WebContents* web_contents;
  if (!CheckAndConvertParams(env, jprofile, jurl, jweb_contents, &url,
                             &prerender_manager, &web_contents))
    return false;

  return prerender_manager->HasPrerenderedAndFinishedLoadingUrl(url,
                                                                web_contents);
}

ExternalPrerenderHandlerAndroid::ExternalPrerenderHandlerAndroid() {}

ExternalPrerenderHandlerAndroid::~ExternalPrerenderHandlerAndroid() {}

static jlong Init(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  ExternalPrerenderHandlerAndroid* external_handler =
      new ExternalPrerenderHandlerAndroid();
  return reinterpret_cast<intptr_t>(external_handler);
}

bool ExternalPrerenderHandlerAndroid::RegisterExternalPrerenderHandlerAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace prerender
