// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TOOLS_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_
#define NET_QUIC_TOOLS_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/core/quic_connection.h"
#include "net/quic/core/quic_packet_writer.h"
#include "net/quic/core/quic_protocol.h"

namespace net {

class IPAddress;
class QuicBlockedWriterInterface;
class UDPServerSocket;
struct WriteResult;


// Chrome specific packet writer which uses a UDPServerSocket for writing
// data.
class QuicSimpleServerPacketWriter : public QuicPacketWriter {
 public:
  typedef base::Callback<void(WriteResult)> WriteCallback;

  QuicSimpleServerPacketWriter(UDPServerSocket* socket,
                               QuicBlockedWriterInterface* blocked_writer);
  ~QuicSimpleServerPacketWriter() override;

  // Wraps WritePacket, and ensures that |callback| is run on successful write.
  WriteResult WritePacketWithCallback(const char* buffer,
                                      size_t buf_len,
                                      const IPAddress& self_address,
                                      const IPEndPoint& peer_address,
                                      PerPacketOptions* options,
                                      WriteCallback callback);

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddress& self_address,
                          const IPEndPoint& peer_address,
                          PerPacketOptions* options) override;

  void OnWriteComplete(int rv);

  // QuicPacketWriter implementation:
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  QuicByteCount GetMaxPacketSize(const IPEndPoint& peer_address) const override;

 private:
  UDPServerSocket* socket_;

  // To be notified after every successful asynchronous write.
  QuicBlockedWriterInterface* blocked_writer_;

  // To call once the write completes.
  WriteCallback callback_;

  // Whether a write is currently in flight.
  bool write_blocked_;

  base::WeakPtrFactory<QuicSimpleServerPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicSimpleServerPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_TOOLS_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_
