// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"

#include <openssl/bytestring.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "crypto/auto_cbb.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace crypto {

namespace {

// Function pointer definition, for injecting the required key export function
// into ExportKeyWithBio, below. |bio| is a temporary memory BIO object, and
// |key| is a handle to the input key object. Return 1 on success, 0 otherwise.
// NOTE: Used with OpenSSL functions, which do not comply with the Chromium
//       style guide, hence the unusual parameter placement / types.
typedef int (*ExportBioFunction)(BIO* bio, const void* key);

using ScopedPKCS8_PRIV_KEY_INFO =
    ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free>;
using ScopedX509_SIG = ScopedOpenSSL<X509_SIG, X509_SIG_free>;

// Helper to export |key| into |output| via the specified ExportBioFunction.
bool ExportKeyWithBio(const void* key,
                      ExportBioFunction export_fn,
                      std::vector<uint8_t>* output) {
  if (!key)
    return false;

  ScopedBIO bio(BIO_new(BIO_s_mem()));
  if (!bio)
    return false;

  if (!export_fn(bio.get(), key))
    return false;

  char* data = nullptr;
  long len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0)
    return false;

  output->assign(data, data + len);
  return true;
}

}  // namespace

ECPrivateKey::~ECPrivateKey() {
  if (key_)
    EVP_PKEY_free(key_);
}

// static
std::unique_ptr<ECPrivateKey> ECPrivateKey::Create() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedEC_KEY ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!ec_key || !EC_KEY_generate_key(ec_key.get()))
    return nullptr;

  std::unique_ptr<ECPrivateKey> result(new ECPrivateKey());
  result->key_ = EVP_PKEY_new();
  if (!result->key_ || !EVP_PKEY_set1_EC_KEY(result->key_, ec_key.get()))
    return nullptr;

  CHECK_EQ(EVP_PKEY_EC, EVP_PKEY_id(result->key_));
  return result;
}

// static
std::unique_ptr<ECPrivateKey> ECPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8_t>& input) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, input.data(), input.size());
  ScopedEVP_PKEY pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0 || EVP_PKEY_id(pkey.get()) != EVP_PKEY_EC)
    return nullptr;

  std::unique_ptr<ECPrivateKey> result(new ECPrivateKey());
  result->key_ = pkey.release();
  return result;
}

// static
std::unique_ptr<ECPrivateKey> ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
    const std::string& password,
    const std::vector<uint8_t>& encrypted_private_key_info,
    const std::vector<uint8_t>& subject_public_key_info) {
  // NOTE: The |subject_public_key_info| can be ignored here, it is only
  // useful for the NSS implementation (which uses the public key's SHA1
  // as a lookup key when storing the private one in its store).
  if (encrypted_private_key_info.empty())
    return nullptr;

  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t* data = &encrypted_private_key_info[0];
  const uint8_t* ptr = data;
  ScopedX509_SIG p8_encrypted(
      d2i_X509_SIG(nullptr, &ptr, encrypted_private_key_info.size()));
  if (!p8_encrypted || ptr != data + encrypted_private_key_info.size())
    return nullptr;

  ScopedPKCS8_PRIV_KEY_INFO p8_decrypted;
  if (password.empty()) {
    // Hack for reading keys generated by an older version of the OpenSSL
    // code. OpenSSL used to use "\0\0" rather than the empty string because it
    // would treat the password as an ASCII string to be converted to UCS-2
    // while NSS used a byte string.
    p8_decrypted.reset(PKCS8_decrypt_pbe(
        p8_encrypted.get(), reinterpret_cast<const uint8_t*>("\0\0"), 2));
  }
  if (!p8_decrypted) {
    p8_decrypted.reset(PKCS8_decrypt_pbe(
        p8_encrypted.get(),
        reinterpret_cast<const uint8_t*>(password.data()),
        password.size()));
  }

  if (!p8_decrypted)
    return nullptr;

  // Create a new EVP_PKEY for it.
  std::unique_ptr<ECPrivateKey> result(new ECPrivateKey());
  result->key_ = EVP_PKCS82PKEY(p8_decrypted.get());
  if (!result->key_ || EVP_PKEY_id(result->key_) != EVP_PKEY_EC)
    return nullptr;

  return result;
}

std::unique_ptr<ECPrivateKey> ECPrivateKey::Copy() const {
  std::unique_ptr<ECPrivateKey> copy(new ECPrivateKey());
  if (key_) {
    EVP_PKEY_up_ref(key_);
    copy->key_ = key_;
  }
  return copy;
}

bool ECPrivateKey::ExportPrivateKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  uint8_t* der;
  size_t der_len;
  AutoCBB cbb;
  if (!CBB_init(cbb.get(), 0) || !EVP_marshal_private_key(cbb.get(), key_) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, der + der_len);
  OPENSSL_free(der);
  return true;
}

bool ECPrivateKey::ExportEncryptedPrivateKey(
    const std::string& password,
    int iterations,
    std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Convert into a PKCS#8 object.
  ScopedPKCS8_PRIV_KEY_INFO pkcs8(EVP_PKEY2PKCS8(key_));
  if (!pkcs8)
    return false;

  // Encrypt the object.
  // NOTE: NSS uses SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC
  // so use NID_pbe_WithSHA1And3_Key_TripleDES_CBC which should be the OpenSSL
  // equivalent.
  ScopedX509_SIG encrypted(PKCS8_encrypt_pbe(
      NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
      nullptr,
      reinterpret_cast<const uint8_t*>(password.data()),
      password.size(),
      nullptr,
      0,
      iterations,
      pkcs8.get()));
  if (!encrypted)
    return false;

  // Write it into |*output|
  return ExportKeyWithBio(encrypted.get(),
                          reinterpret_cast<ExportBioFunction>(i2d_PKCS8_bio),
                          output);
}

bool ECPrivateKey::ExportPublicKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  uint8_t *der;
  size_t der_len;
  AutoCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), key_) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, der + der_len);
  OPENSSL_free(der);
  return true;
}

bool ECPrivateKey::ExportRawPublicKey(std::string* output) const {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Export the x and y field elements as 32-byte, big-endian numbers. (This is
  // the same as X9.62 uncompressed form without the leading 0x04 byte.)
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key_);
  ScopedBIGNUM x(BN_new());
  ScopedBIGNUM y(BN_new());
  uint8_t buf[64];
  if (!x || !y ||
      !EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec_key),
                                           EC_KEY_get0_public_key(ec_key),
                                           x.get(), y.get(), nullptr) ||
      !BN_bn2bin_padded(buf, 32, x.get()) ||
      !BN_bn2bin_padded(buf + 32, 32, y.get())) {
    return false;
  }

  output->assign(reinterpret_cast<const char*>(buf), sizeof(buf));
  return true;
}

ECPrivateKey::ECPrivateKey() : key_(nullptr) {}

}  // namespace crypto
