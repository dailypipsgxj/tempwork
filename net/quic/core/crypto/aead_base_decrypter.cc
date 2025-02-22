// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/err.h>
#include <openssl/evp.h>

#include <memory>

#include "net/quic/core/crypto/aead_base_decrypter.h"
#include "net/quic/core/quic_bug_tracker.h"
#include "net/quic/core/quic_flags.h"
#include "net/quic/core/quic_utils.h"

using base::StringPiece;
using std::string;

namespace net {

namespace {

// Clear OpenSSL error stack.
void ClearOpenSslErrors() {
  while (ERR_get_error()) {
  }
}

// In debug builds only, log OpenSSL error stack. Then clear OpenSSL error
// stack.
void DLogOpenSslErrors() {
#ifdef NDEBUG
  ClearOpenSslErrors();
#else
  while (uint32_t error = ERR_get_error()) {
    char buf[120];
    ERR_error_string_n(error, buf, arraysize(buf));
    DLOG(ERROR) << "OpenSSL error: " << buf;
  }
#endif
}

}  // namespace

AeadBaseDecrypter::AeadBaseDecrypter(const EVP_AEAD* aead_alg,
                                     size_t key_size,
                                     size_t auth_tag_size,
                                     size_t nonce_prefix_size)
    : aead_alg_(aead_alg),
      key_size_(key_size),
      auth_tag_size_(auth_tag_size),
      nonce_prefix_size_(nonce_prefix_size),
      have_preliminary_key_(false) {
  DCHECK_GT(256u, key_size);
  DCHECK_GT(256u, auth_tag_size);
  DCHECK_GT(256u, nonce_prefix_size);
  DCHECK_LE(key_size_, sizeof(key_));
  DCHECK_LE(nonce_prefix_size_, sizeof(nonce_prefix_));
}

AeadBaseDecrypter::~AeadBaseDecrypter() {}

bool AeadBaseDecrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), key_size_);
  if (key.size() != key_size_) {
    return false;
  }
  memcpy(key_, key.data(), key.size());

  EVP_AEAD_CTX_cleanup(ctx_.get());
  if (!EVP_AEAD_CTX_init(ctx_.get(), aead_alg_, key_, key_size_, auth_tag_size_,
                         nullptr)) {
    DLogOpenSslErrors();
    return false;
  }

  return true;
}

bool AeadBaseDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), nonce_prefix_size_);
  if (nonce_prefix.size() != nonce_prefix_size_) {
    return false;
  }
  memcpy(nonce_prefix_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool AeadBaseDecrypter::SetPreliminaryKey(StringPiece key) {
  DCHECK(!have_preliminary_key_);
  SetKey(key);
  have_preliminary_key_ = true;

  return true;
}

bool AeadBaseDecrypter::SetDiversificationNonce(DiversificationNonce nonce) {
  if (!have_preliminary_key_) {
    return true;
  }

  string key, nonce_prefix;
  DiversifyPreliminaryKey(
      StringPiece(reinterpret_cast<const char*>(key_), key_size_),
      StringPiece(reinterpret_cast<const char*>(nonce_prefix_),
                  nonce_prefix_size_),
      nonce, key_size_, nonce_prefix_size_, &key, &nonce_prefix);

  if (!SetKey(key) || !SetNoncePrefix(nonce_prefix)) {
    DCHECK(false);
    return false;
  }

  have_preliminary_key_ = false;
  return true;
}

bool AeadBaseDecrypter::DecryptPacket(QuicPathId path_id,
                                      QuicPacketNumber packet_number,
                                      StringPiece associated_data,
                                      StringPiece ciphertext,
                                      char* output,
                                      size_t* output_length,
                                      size_t max_output_length) {
  if (ciphertext.length() < auth_tag_size_) {
    return false;
  }

  if (have_preliminary_key_) {
    QUIC_BUG << "Unable to decrypt while key diversification is pending";
    return false;
  }

  uint8_t nonce[sizeof(nonce_prefix_) + sizeof(packet_number)];
  const size_t nonce_size = nonce_prefix_size_ + sizeof(packet_number);
  memcpy(nonce, nonce_prefix_, nonce_prefix_size_);
  uint64_t path_id_packet_number =
      QuicUtils::PackPathIdAndPacketNumber(path_id, packet_number);
  memcpy(nonce + nonce_prefix_size_, &path_id_packet_number,
         sizeof(path_id_packet_number));
  if (!EVP_AEAD_CTX_open(
          ctx_.get(), reinterpret_cast<uint8_t*>(output), output_length,
          max_output_length, reinterpret_cast<const uint8_t*>(nonce),
          nonce_size, reinterpret_cast<const uint8_t*>(ciphertext.data()),
          ciphertext.size(),
          reinterpret_cast<const uint8_t*>(associated_data.data()),
          associated_data.size())) {
    // Because QuicFramer does trial decryption, decryption errors are expected
    // when encryption level changes. So we don't log decryption errors.
    ClearOpenSslErrors();
    return false;
  }
  return true;
}

StringPiece AeadBaseDecrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), key_size_);
}

StringPiece AeadBaseDecrypter::GetNoncePrefix() const {
  if (nonce_prefix_size_ == 0) {
    return StringPiece();
  }
  return StringPiece(reinterpret_cast<const char*>(nonce_prefix_),
                     nonce_prefix_size_);
}

}  // namespace net
