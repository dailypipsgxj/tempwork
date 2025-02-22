// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/core/network_resources.h"
#include "components/sync/core/read_transaction.h"
#include "components/sync_driver/about_sync_util.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/components_strings.h"
#include "jni/ProfileSyncService_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Native callback for the JNI GetAllNodes method. When
// ProfileSyncService::GetAllNodes completes, this method is called and the
// results are sent to the Java callback.
void NativeGetAllNodesCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    std::unique_ptr<base::ListValue> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json_string;
  if (!result.get() || !base::JSONWriter::Write(*result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  ScopedJavaLocalRef<jstring> java_json_string =
      ConvertUTF8ToJavaString(env, json_string);
  Java_ProfileSyncService_onGetAllNodesResult(env,
                                              callback.obj(),
                                              java_json_string.obj());
}

ScopedJavaLocalRef<jintArray> ModelTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::ModelTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    type_vector.push_back(it.Get());
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

}  // namespace

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      sync_service_(NULL),
      weak_java_profile_sync_service_(env, obj) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = ProfileManager::GetActiveUserProfile();
  if (profile_ == NULL) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_prefs_.reset(new sync_driver::SyncPrefs(profile_->GetPrefs()));

  sync_service_ =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
}

bool ProfileSyncServiceAndroid::Init() {
  if (sync_service_) {
    sync_service_->AddObserver(this);
    sync_service_->SetPlatformSyncAllowedProvider(
        base::Bind(&ProfileSyncServiceAndroid::IsSyncAllowedByAndroid,
                   base::Unretained(this)));
    return true;
  } else {
    return false;
  }
}

ProfileSyncServiceAndroid::~ProfileSyncServiceAndroid() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
    sync_service_->SetPlatformSyncAllowedProvider(
        ProfileSyncService::PlatformSyncAllowedProvider());
  }
}

void ProfileSyncServiceAndroid::OnStateChanged() {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_ProfileSyncService_syncStateChanged(
      env, weak_java_profile_sync_service_.get(env).obj());
}

bool ProfileSyncServiceAndroid::IsSyncAllowedByAndroid() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  return Java_ProfileSyncService_isMasterSyncEnabled(
      env, weak_java_profile_sync_service_.get(env).obj());
}

// Pure ProfileSyncService calls.

jboolean ProfileSyncServiceAndroid::IsSyncRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncRequested();
}

void ProfileSyncServiceAndroid::RequestStart(JNIEnv* env,
                                             const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStart();
}

void ProfileSyncServiceAndroid::RequestStop(JNIEnv* env,
                                            const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->RequestStop(ProfileSyncService::KEEP_DATA);
}

void ProfileSyncServiceAndroid::SignOutSync(JNIEnv* env,
                                            const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  sync_service_->RequestStop(ProfileSyncService::CLEAR_DATA);
}

jboolean ProfileSyncServiceAndroid::IsSyncActive(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsSyncActive();
}

jboolean ProfileSyncServiceAndroid::IsBackendInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsBackendInitialized();
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean in_progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (in_progress) {
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  } else {
    sync_blocker_.reset();
  }
}

jboolean ProfileSyncServiceAndroid::IsFirstSetupComplete(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsFirstSetupComplete();
}

void ProfileSyncServiceAndroid::SetFirstSetupComplete(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->SetFirstSetupComplete();
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetActiveDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  syncer::ModelTypeSet types = sync_service_->GetActiveDataTypes();
  return ModelTypeSetToJavaIntArray(env, types);
}

ScopedJavaLocalRef<jintArray> ProfileSyncServiceAndroid::GetPreferredDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  return ModelTypeSetToJavaIntArray(env, types);
}

void ProfileSyncServiceAndroid::SetPreferredDataTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& model_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, model_type_array, &types_vector);
  syncer::ModelTypeSet types;
  for (size_t i = 0; i < types_vector.size(); i++) {
    types.Put(static_cast<syncer::ModelType>(types_vector[i]));
  }
  sync_service_->OnUserChoseDatatypes(sync_everything, types);
}

jboolean ProfileSyncServiceAndroid::IsCryptographerReady(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  return sync_service_->IsCryptographerReady(&trans);
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEncryptEverythingAllowed();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsEncryptEverythingEnabled();
}

void ProfileSyncServiceAndroid::EnableEncryptEverything(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->EnableEncryptEverything();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForDecryption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsPassphraseRequiredForDecryption();
}

jboolean ProfileSyncServiceAndroid::IsUsingSecondaryPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->IsUsingSecondaryPassphrase();
}

ScopedJavaLocalRef<jbyteArray>
ProfileSyncServiceAndroid::GetCustomPassphraseKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::string key = sync_service_->GetCustomPassphraseKey();
  return base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(key.data()), key.size());
}

jint ProfileSyncServiceAndroid::GetPassphraseType(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetPassphraseType();
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->SetEncryptionPassphrase(key, ProfileSyncService::EXPLICIT);
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->SetDecryptionPassphrase(key);
}

jboolean ProfileSyncServiceAndroid::HasExplicitPassphraseTime(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return !passphrase_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetExplicitPassphraseTime(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return passphrase_time.ToJavaTime();
}

void ProfileSyncServiceAndroid::FlushDirectory(JNIEnv* env,
                                               const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_->FlushDirectory();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::QuerySyncStatusSummary(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  std::string status(sync_service_->QuerySyncStatusSummaryString());
  return ConvertUTF8ToJavaString(env, status);
}

void ProfileSyncServiceAndroid::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);

  base::Callback<void(std::unique_ptr<base::ListValue>)> native_callback =
      base::Bind(&NativeGetAllNodesCallback, java_callback);
  sync_service_->GetAllNodes(native_callback);
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env,
                                             const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_->HasUnrecoverableError();
}

// Pure SyncPrefs calls.

jboolean ProfileSyncServiceAndroid::IsPassphrasePrompted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return sync_prefs_->IsPassphrasePrompted();
}

void ProfileSyncServiceAndroid::SetPassphrasePrompted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean prompted) {
  sync_prefs_->SetPassphrasePrompted(prompted);
}

void ProfileSyncServiceAndroid::SetSyncSessionsId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile_);
  std::string machine_tag = ConvertJavaStringToUTF8(env, tag);
  sync_prefs_->SetSyncSessionsGUID(machine_tag);
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_prefs_->HasKeepEverythingSynced();
}

// UI string getters.

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterGooglePassphraseBodyWithDateText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
        IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyWithDateText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetCurrentSignedInAccountText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountInfo()
          .email;
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          base::ASCIIToUTF16(sync_username)));
}

ScopedJavaLocalRef<jstring>
ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyText(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_SYNC_ENTER_PASSPHRASE_BODY));
}

// Functionality only available for testing purposes.

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::GetAboutInfoForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<base::DictionaryValue> about_info =
      sync_driver::sync_ui_util::ConstructAboutInformation(
          sync_service_, sync_service_->signin(), chrome::GetChannel());
  std::string about_info_json;
  base::JSONWriter::Write(*about_info, &about_info_json);

  return ConvertUTF8ToJavaString(env, about_info_json);
}

jlong ProfileSyncServiceAndroid::GetLastSyncedTimeForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // Use profile preferences here instead of SyncPrefs to avoid an extra
  // conversion, since SyncPrefs::GetLastSyncedTime() converts the stored value
  // to to base::Time.
  return static_cast<jlong>(
      profile_->GetPrefs()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
}

void ProfileSyncServiceAndroid::OverrideNetworkResourcesForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong network_resources) {
  syncer::NetworkResources* resources =
      reinterpret_cast<syncer::NetworkResources*>(network_resources);
  sync_service_->OverrideNetworkResourcesForTest(
      base::WrapUnique<syncer::NetworkResources>(resources));
}

// static
ProfileSyncServiceAndroid*
    ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid() {
  return reinterpret_cast<ProfileSyncServiceAndroid*>(
      Java_ProfileSyncService_getProfileSyncServiceAndroid(
          AttachCurrentThread()));
}

// static
bool ProfileSyncServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  if (profile_sync_service_android->Init()) {
    return reinterpret_cast<intptr_t>(profile_sync_service_android);
  } else {
    delete profile_sync_service_android;
    return 0;
  }
}
