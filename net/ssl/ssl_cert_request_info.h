// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CERT_REQUEST_INFO_H_
#define NET_SSL_SSL_CERT_REQUEST_INFO_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/ssl/ssl_client_cert_type.h"

namespace net {

class X509Certificate;

// The SSLCertRequestInfo class represents server criteria regarding client
// certificate required for a secure connection.
//
// In TLS 1.1, the CertificateRequest
// message is defined as:
//   enum {
//   rsa_sign(1), dss_sign(2), rsa_fixed_dh(3), dss_fixed_dh(4),
//   rsa_ephemeral_dh_RESERVED(5), dss_ephemeral_dh_RESERVED(6),
//   fortezza_dms_RESERVED(20), (255)
//   } ClientCertificateType;
//
//   opaque DistinguishedName<1..2^16-1>;
//
//   struct {
//       ClientCertificateType certificate_types<1..2^8-1>;
//       DistinguishedName certificate_authorities<0..2^16-1>;
//   } CertificateRequest;
class NET_EXPORT SSLCertRequestInfo
    : public base::RefCountedThreadSafe<SSLCertRequestInfo> {
 public:
  SSLCertRequestInfo();

  void Reset();

  // The host and port of the SSL server that requested client authentication.
  HostPortPair host_and_port;

  // True if the server that issues this request was the HTTPS proxy used in
  // the request.  False, if the server was the origin server.
  bool is_proxy;

  // List of DER-encoded X.509 DistinguishedName of certificate authorities
  // allowed by the server.
  std::vector<std::string> cert_authorities;

  std::vector<SSLClientCertType> cert_key_types;

  // Client certificates matching the server criteria. This should be removed
  // soon as being tracked in http://crbug.com/166642.
  std::vector<scoped_refptr<X509Certificate> > client_certs;

 private:
  friend class base::RefCountedThreadSafe<SSLCertRequestInfo>;

  ~SSLCertRequestInfo();
};

}  // namespace net

#endif  // NET_SSL_SSL_CERT_REQUEST_INFO_H_
