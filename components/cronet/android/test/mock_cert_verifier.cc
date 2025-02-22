// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_cert_verifier.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/test/test_support_android.h"
#include "crypto/sha2.h"
#include "jni/MockCertVerifier_jni.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

namespace cronet {

namespace {

// Populates |out_hash_value| with the SHA256 hash of the |cert| public key.
// Returns true on success.
static bool CalculatePublicKeySha256(const net::X509Certificate& cert,
                                     net::HashValue* out_hash_value) {
  // Convert the cert to DER encoded bytes.
  std::string der_cert_bytes;
  net::X509Certificate::OSCertHandle cert_handle = cert.os_cert_handle();
  if (!net::X509Certificate::GetDEREncoded(cert_handle, &der_cert_bytes)) {
    LOG(INFO) << "Unable to convert the given cert to DER encoding";
    return false;
  }
  // Extract the public key from the cert.
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(der_cert_bytes, &spki_bytes)) {
    LOG(INFO) << "Unable to retrieve the public key from the DER cert";
    return false;
  }
  // Calculate SHA256 hash of public key bytes.
  out_hash_value->tag = net::HASH_VALUE_SHA256;
  crypto::SHA256HashString(spki_bytes, out_hash_value->data(),
                           crypto::kSHA256Length);
  return true;
}

}  // namespace

static jlong CreateMockCertVerifier(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobjectArray>& jcerts,
    const jboolean jknown_root,
    const JavaParamRef<jstring>& jtest_data_dir) {
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  std::vector<std::string> certs;
  base::android::AppendJavaStringArrayToStringVector(env, jcerts, &certs);
  net::MockCertVerifier* mock_cert_verifier = new net::MockCertVerifier();
  for (const auto& cert : certs) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert);

    // By default, HPKP verification is enabled for known trust roots only.
    verify_result.is_issued_by_known_root = jknown_root;

    // Calculate the public key hash and add it to the verify_result.
    net::HashValue hashValue;
    CHECK(CalculatePublicKeySha256(*verify_result.verified_cert.get(),
                                   &hashValue));
    verify_result.public_key_hashes.push_back(hashValue);

    mock_cert_verifier->AddResultForCert(verify_result.verified_cert.get(),
                                         verify_result, net::OK);
  }

  return reinterpret_cast<jlong>(mock_cert_verifier);
}

bool RegisterMockCertVerifier(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
