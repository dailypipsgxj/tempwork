// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_piece.h"
#include "net/quic/core/crypto/crypto_framer.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece crypto_input(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<net::CryptoHandshakeMessage> handshake_message(
      net::CryptoFramer::ParseMessage(crypto_input));

  return 0;
}
