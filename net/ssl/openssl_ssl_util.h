// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_SSL_UTIL_H_
#define NET_SSL_OPENSSL_SSL_UTIL_H_

#include <stdint.h>

#include <openssl/ssl.h>

#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/ssl/scoped_openssl_types.h"

namespace crypto {
class OpenSSLErrStackTracer;
}

namespace tracked_objects {
class Location;
}

namespace net {

// Puts a net error, |err|, on the error stack in OpenSSL. The file and line are
// extracted from |posted_from|. The function code of the error is left as 0.
void OpenSSLPutNetError(const tracked_objects::Location& posted_from, int err);

// Utility to construct the appropriate set & clear masks for use the OpenSSL
// options and mode configuration functions. (SSL_set_options etc)
struct SslSetClearMask {
  SslSetClearMask();
  void ConfigureFlag(long flag, bool state);

  long set_mask;
  long clear_mask;
};

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed.
//
// Note that |tracer| is not currently used in the implementation, but is passed
// in anyway as this ensures the caller will clear any residual codes left on
// the error stack.
int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer);

// Helper struct to store information about an OpenSSL error stack entry.
struct OpenSSLErrorInfo {
  OpenSSLErrorInfo() : error_code(0), file(NULL), line(0) {}

  uint32_t error_code;
  const char* file;
  int line;
};

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed. If a value on the stack is used, the error code and
// associated information are returned in |*out_error_info|. Otherwise its
// fields are set to 0 and NULL. This function will never return OK, so
// SSL_ERROR_ZERO_RETURN must be handled externally.
//
// Note that |tracer| is not currently used in the implementation, but is passed
// in anyway as this ensures the caller will clear any residual codes left on
// the error stack.
int MapOpenSSLErrorWithDetails(int err,
                               const crypto::OpenSSLErrStackTracer& tracer,
                               OpenSSLErrorInfo* out_error_info);

// Creates NetLog callback for an OpenSSL error.
NetLog::ParametersCallback CreateNetLogOpenSSLErrorCallback(
    int net_error,
    int ssl_error,
    const OpenSSLErrorInfo& error_info);

// Returns the net SSL version number (see ssl_connection_status_flags.h) for
// this SSL connection.
int GetNetSSLVersion(SSL* ssl);

ScopedX509 OSCertHandleToOpenSSL(X509Certificate::OSCertHandle os_handle);

ScopedX509Stack OSCertHandlesToOpenSSL(
    const X509Certificate::OSCertHandles& os_handles);

}  // namespace net

#endif  // NET_SSL_OPENSSL_SSL_UTIL_H_
