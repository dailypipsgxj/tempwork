// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/DomDistillerUIUtils_jni.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace android {

// static
void DistillerUIHandleAndroid::ReportExternalFeedback(
    content::WebContents* web_contents,
    const GURL& url,
    const bool good) {
  if (!web_contents)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl = base::android::ConvertUTF8ToJavaString(
      env, url_utils::GetOriginalUrlFromDistillerUrl(url).spec());

  Java_DomDistillerUIUtils_reportFeedbackWithWebContents(
      env, web_contents->GetJavaWebContents().obj(), jurl.obj(), good);
}

// static
void DistillerUIHandleAndroid::OpenSettings(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DomDistillerUIUtils_openSettings(env,
      web_contents->GetJavaWebContents().obj());
}

// static
void DistillerUIHandleAndroid::ClosePanel(bool animate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DomDistillerUIUtils_closePanel(env, animate);
}

}  // namespace android

}  // namespace dom_distiller
