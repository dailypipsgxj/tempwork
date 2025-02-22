// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROPERTIES_BASED_QUIC_SERVER_INFO_H_
#define NET_QUIC_CRYPTO_PROPERTIES_BASED_QUIC_SERVER_INFO_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/quic/core/crypto/quic_server_info.h"

namespace net {

class HttpServerProperties;

// PropertiesBasedQuicServerInfo fetches information about a QUIC server from
// HttpServerProperties. Since the information is defined to be non-sensitive,
// it's ok for us to keep it on disk.
class NET_EXPORT_PRIVATE PropertiesBasedQuicServerInfo : public QuicServerInfo {
 public:
  PropertiesBasedQuicServerInfo(const QuicServerId& server_id,
                                HttpServerProperties* http_server_properties);
  ~PropertiesBasedQuicServerInfo() override;

  // QuicServerInfo implementation.
  void Start() override;
  int WaitForDataReady(const CompletionCallback& callback) override;
  void ResetWaitForDataReadyCallback() override;
  void CancelWaitForDataReadyCallback() override;
  bool IsDataReady() override;
  bool IsReadyToPersist() override;
  void Persist() override;
  void OnExternalCacheHit() override;

 private:
  HttpServerProperties* http_server_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertiesBasedQuicServerInfo);
};

class NET_EXPORT_PRIVATE PropertiesBasedQuicServerInfoFactory
    : public QuicServerInfoFactory {
 public:
  explicit PropertiesBasedQuicServerInfoFactory(
      HttpServerProperties* http_server_properties);
  ~PropertiesBasedQuicServerInfoFactory() override;

  QuicServerInfo* GetForServer(const QuicServerId& server_id) override;

 private:
  HttpServerProperties* http_server_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertiesBasedQuicServerInfoFactory);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROPERTIES_BASED_QUIC_SERVER_INFO_H_
