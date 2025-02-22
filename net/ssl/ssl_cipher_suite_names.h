// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CIPHER_SUITE_NAMES_H_
#define NET_SSL_SSL_CIPHER_SUITE_NAMES_H_

#include <stdint.h>

#include <string>

#include "net/base/net_export.h"

namespace net {

// SSLCipherSuiteToStrings returns three strings for a given cipher suite
// number, the name of the key exchange algorithm, the name of the cipher and
// the name of the MAC. The cipher suite number is the number as sent on the
// wire and recorded at
// http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
// If the cipher suite is unknown, the strings are set to "???".
// In the case of an AEAD cipher suite, *mac_str is NULL and *is_aead is true.
NET_EXPORT void SSLCipherSuiteToStrings(const char** key_exchange_str,
                                        const char** cipher_str,
                                        const char** mac_str,
                                        bool* is_aead,
                                        uint16_t cipher_suite);

// SSLVersionToString returns the name of the SSL protocol version
// specified by |ssl_version|, which is defined in
// net/ssl/ssl_connection_status_flags.h.
// If the version is unknown, |name| is set to "???".
NET_EXPORT void SSLVersionToString(const char** name, int ssl_version);

// Parses a string literal that represents a SSL/TLS cipher suite.
//
// Supported literal forms:
//   0xAABB, where AA is cipher_suite[0] and BB is cipher_suite[1], as
//     defined in RFC 2246, Section 7.4.1.2. Unrecognized but parsable cipher
//     suites in this form will not return an error.
//
// Returns true if the cipher suite was successfully parsed, storing the
// result in |cipher_suite|.
//
// TODO(rsleevi): Support the full strings defined in the IANA TLS parameters
// list.
NET_EXPORT bool ParseSSLCipherString(const std::string& cipher_string,
                                     uint16_t* cipher_suite);

// |cipher_suite| is the IANA id for the cipher suite. What a "secure"
// cipher suite is arbitrarily determined here. The intent is to indicate what
// cipher suites meet modern security standards when backwards compatibility can
// be ignored.
//
// Currently, this function follows these criteria:
// 1) Only uses ECDHE-based key exchanges authenticated by a certificate
// 2) Only uses AEADs
NET_EXPORT bool IsSecureTLSCipherSuite(uint16_t cipher_suite);

// Returns true if |cipher_suite| is suitable for use with HTTP/2. See
// https://http2.github.io/http2-spec/#rfc.section.9.2.2.
NET_EXPORT bool IsTLSCipherSuiteAllowedByHTTP2(uint16_t cipher_suite);

// Returns the static curve name of |key_exchange_info| if the |cipher_suite|
// is an elliptic curve, and a name is known. Returns nullptr otherwise.
// Only defined for OpenSSL, returns nullptr otherwise.
NET_EXPORT const char* ECCurveName(uint16_t cipher_suite,
                                   int key_exchange_info);

}  // namespace net

#endif  // NET_SSL_SSL_CIPHER_SUITE_NAMES_H_
