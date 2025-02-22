// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_native_dialog_factory.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/javascript_message_type.h"
#include "jni/JavascriptAppModalDialog_jni.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

JavascriptAppModalDialogAndroid::JavascriptAppModalDialogAndroid(
    JNIEnv* env,
    app_modal::JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent)
    : dialog_(dialog),
      parent_jobject_weak_ref_(env, parent->GetJavaObject().obj()) {
}

int JavascriptAppModalDialogAndroid::GetAppModalDialogButtons() const {
  NOTIMPLEMENTED();
  return 0;
}

void JavascriptAppModalDialogAndroid::ShowAppModalDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();
  // Keep a strong ref to the parent window while we make the call to java to
  // display the dialog.
  ScopedJavaLocalRef<jobject> parent_jobj = parent_jobject_weak_ref_.get(env);
  if (parent_jobj.is_null()) {
    CancelAppModalDialog();
    return;
  }

  ScopedJavaLocalRef<jobject> dialog_object;
  ScopedJavaLocalRef<jstring> title =
      ConvertUTF16ToJavaString(env, dialog_->title());
  ScopedJavaLocalRef<jstring> message =
      ConvertUTF16ToJavaString(env, dialog_->message_text());

  switch (dialog_->javascript_message_type()) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT: {
      dialog_object = Java_JavascriptAppModalDialog_createAlertDialog(env,
          title.obj(), message.obj(),
          dialog_->display_suppress_checkbox());
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM: {
      if (dialog_->is_before_unload_dialog()) {
        dialog_object = Java_JavascriptAppModalDialog_createBeforeUnloadDialog(
            env, title.obj(), message.obj(), dialog_->is_reload(),
            dialog_->display_suppress_checkbox());
      } else {
        dialog_object = Java_JavascriptAppModalDialog_createConfirmDialog(env,
            title.obj(), message.obj(),
            dialog_->display_suppress_checkbox());
      }
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> default_prompt_text =
          ConvertUTF16ToJavaString(env, dialog_->default_prompt_text());
      dialog_object = Java_JavascriptAppModalDialog_createPromptDialog(env,
          title.obj(), message.obj(),
          dialog_->display_suppress_checkbox(), default_prompt_text.obj());
      break;
    }
    default:
      NOTREACHED();
  }

  // Keep a ref to the java side object until we get a confirm or cancel.
  dialog_jobject_.Reset(dialog_object);

  Java_JavascriptAppModalDialog_showJavascriptAppModalDialog(env,
      dialog_object.obj(), parent_jobj.obj(),
      reinterpret_cast<intptr_t>(this));
}

void JavascriptAppModalDialogAndroid::ActivateAppModalDialog() {
  ShowAppModalDialog();
}

void JavascriptAppModalDialogAndroid::CloseAppModalDialog() {
  CancelAppModalDialog();
}

void JavascriptAppModalDialogAndroid::AcceptAppModalDialog() {
  base::string16 prompt_text;
  dialog_->OnAccept(prompt_text, false);
  delete this;
}

void JavascriptAppModalDialogAndroid::DidAcceptAppModalDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const JavaParamRef<jstring>& prompt,
    bool should_suppress_js_dialogs) {
  base::string16 prompt_text =
      base::android::ConvertJavaStringToUTF16(env, prompt);
  dialog_->OnAccept(prompt_text, should_suppress_js_dialogs);
  delete this;
}

void JavascriptAppModalDialogAndroid::CancelAppModalDialog() {
  dialog_->OnCancel(false);
  delete this;
}

bool JavascriptAppModalDialogAndroid::IsShowing() const {
  return true;
}

void JavascriptAppModalDialogAndroid::DidCancelAppModalDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    bool should_suppress_js_dialogs) {
  dialog_->OnCancel(should_suppress_js_dialogs);
  delete this;
}

const ScopedJavaGlobalRef<jobject>&
    JavascriptAppModalDialogAndroid::GetDialogObject() const {
  return dialog_jobject_;
}

// static
ScopedJavaLocalRef<jobject> GetCurrentModalDialog(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  app_modal::AppModalDialog* dialog =
      app_modal::AppModalDialogQueue::GetInstance()->active_dialog();
  if (!dialog || !dialog->native_dialog())
    return ScopedJavaLocalRef<jobject>();

  JavascriptAppModalDialogAndroid* js_dialog =
      static_cast<JavascriptAppModalDialogAndroid*>(dialog->native_dialog());
  return ScopedJavaLocalRef<jobject>(js_dialog->GetDialogObject());
}

// static
bool JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

JavascriptAppModalDialogAndroid::~JavascriptAppModalDialogAndroid() {
  // In case the dialog is still displaying, tell it to close itself.
  // This can happen if you trigger a dialog but close the Tab before it's
  // shown, and then accept the dialog.
  if (!dialog_jobject_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_JavascriptAppModalDialog_dismiss(env, dialog_jobject_.obj());
  }
}

namespace {

class ChromeJavaScriptNativeDialogAndroidFactory
    : public app_modal::JavaScriptNativeDialogFactory {
 public:
  ChromeJavaScriptNativeDialogAndroidFactory() {}
  ~ChromeJavaScriptNativeDialogAndroidFactory() override {}

 private:
  app_modal::NativeAppModalDialog* CreateNativeJavaScriptDialog(
      app_modal::JavaScriptAppModalDialog* dialog) override {
    dialog->web_contents()->GetDelegate()->ActivateContents(
        dialog->web_contents());
    return new JavascriptAppModalDialogAndroid(
        base::android::AttachCurrentThread(),
        dialog, dialog->web_contents()->GetTopLevelNativeWindow());
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptNativeDialogAndroidFactory);
};

}  // namespace

void InstallChromeJavaScriptNativeDialogFactory() {
  app_modal::JavaScriptDialogManager::GetInstance()->SetNativeDialogFactory(
      base::WrapUnique(new ChromeJavaScriptNativeDialogAndroidFactory));
}

