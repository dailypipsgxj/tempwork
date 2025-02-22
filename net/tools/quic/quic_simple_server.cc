// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server.h"

#include <string.h>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/quic/core/crypto/crypto_handshake.h"
#include "net/quic/core/crypto/quic_random.h"
#include "net/quic/core/quic_crypto_stream.h"
#include "net/quic/core/quic_data_reader.h"
#include "net/quic/core/quic_protocol.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_simple_per_connection_packet_writer.h"
#include "net/tools/quic/quic_simple_server_packet_writer.h"
#include "net/tools/quic/quic_simple_server_session_helper.h"
#include "net/udp/udp_server_socket.h"

namespace net {

namespace {

const char kSourceAddressTokenSecret[] = "secret";

// Allocate some extra space so we can send an error if the client goes over
// the limit.
const int kReadBufferSize = 2 * kMaxPacketSize;

class SimpleQuicDispatcher : public QuicDispatcher {
 public:
  SimpleQuicDispatcher(const QuicConfig& config,
                       const QuicCryptoServerConfig* crypto_config,
                       const QuicVersionVector& supported_versions,
                       QuicConnectionHelperInterface* helper,
                       QuicAlarmFactory* alarm_factory)
      : QuicDispatcher(
            config,
            crypto_config,
            supported_versions,
            std::unique_ptr<QuicConnectionHelperInterface>(helper),
            std::unique_ptr<QuicServerSessionBase::Helper>(
                new QuicSimpleServerSessionHelper(QuicRandom::GetInstance())),
            std::unique_ptr<QuicAlarmFactory>(alarm_factory)) {}

 protected:
  QuicServerSessionBase* CreateQuicSession(
      QuicConnectionId connection_id,
      const IPEndPoint& client_address) override {
    QuicServerSessionBase* session =
        QuicDispatcher::CreateQuicSession(connection_id, client_address);
    static_cast<QuicSimplePerConnectionPacketWriter*>(
        session->connection()->writer())
        ->set_connection(session->connection());
    return session;
  }

  QuicPacketWriter* CreatePerConnectionWriter() override {
    return new QuicSimplePerConnectionPacketWriter(
        static_cast<QuicSimpleServerPacketWriter*>(writer()));
  }
};

}  // namespace

QuicSimpleServer::QuicSimpleServer(std::unique_ptr<ProofSource> proof_source,
                                   const QuicConfig& config,
                                   const QuicVersionVector& supported_versions)
    : helper_(
          new QuicChromiumConnectionHelper(&clock_, QuicRandom::GetInstance())),
      alarm_factory_(new QuicChromiumAlarmFactory(
          base::ThreadTaskRunnerHandle::Get().get(),
          &clock_)),
      config_(config),
      crypto_config_(kSourceAddressTokenSecret,
                     QuicRandom::GetInstance(),
                     std::move(proof_source)),
      supported_versions_(supported_versions),
      read_pending_(false),
      synchronous_read_count_(0),
      read_buffer_(new IOBufferWithSize(kReadBufferSize)),
      weak_factory_(this) {
  Initialize();
}

void QuicSimpleServer::Initialize() {
#if MMSG_MORE
  use_recvmmsg_ = true;
#endif

  // If an initial flow control window has not explicitly been set, then use a
  // sensible value for a server: 1 MB for session, 64 KB for each stream.
  const uint32_t kInitialSessionFlowControlWindow = 1 * 1024 * 1024;  // 1 MB
  const uint32_t kInitialStreamFlowControlWindow = 64 * 1024;         // 64 KB
  if (config_.GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindow);
  }
  if (config_.GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindow);
  }

  std::unique_ptr<CryptoHandshakeMessage> scfg(crypto_config_.AddDefaultConfig(
      helper_->GetRandomGenerator(), helper_->GetClock(),
      QuicCryptoServerConfig::ConfigOptions()));
}

QuicSimpleServer::~QuicSimpleServer() {}

int QuicSimpleServer::Listen(const IPEndPoint& address) {
  std::unique_ptr<UDPServerSocket> socket(
      new UDPServerSocket(&net_log_, NetLog::Source()));

  socket->AllowAddressReuse();

  int rc = socket->Listen(address);
  if (rc < 0) {
    LOG(ERROR) << "Listen() failed: " << ErrorToString(rc);
    return rc;
  }

  // These send and receive buffer sizes are sized for a single connection,
  // because the default usage of QuicSimpleServer is as a test server with
  // one or two clients.  Adjust higher for use with many clients.
  rc = socket->SetReceiveBufferSize(
      static_cast<int32_t>(kDefaultSocketReceiveBuffer));
  if (rc < 0) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToString(rc);
    return rc;
  }

  rc = socket->SetSendBufferSize(20 * kMaxPacketSize);
  if (rc < 0) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToString(rc);
    return rc;
  }

  rc = socket->GetLocalAddress(&server_address_);
  if (rc < 0) {
    LOG(ERROR) << "GetLocalAddress() failed: " << ErrorToString(rc);
    return rc;
  }

  DVLOG(1) << "Listening on " << server_address_.ToString();

  socket_.swap(socket);

  dispatcher_.reset(new SimpleQuicDispatcher(
      config_, &crypto_config_, supported_versions_, helper_, alarm_factory_));
  QuicSimpleServerPacketWriter* writer =
      new QuicSimpleServerPacketWriter(socket_.get(), dispatcher_.get());
  dispatcher_->InitializeWithWriter(writer);

  StartReading();

  return OK;
}

void QuicSimpleServer::Shutdown() {
  // Before we shut down the epoll server, give all active sessions a chance to
  // notify clients that they're closing.
  dispatcher_->Shutdown();

  socket_->Close();
  socket_.reset();
}

void QuicSimpleServer::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;

  int result = socket_->RecvFrom(
      read_buffer_.get(), read_buffer_->size(), &client_address_,
      base::Bind(&QuicSimpleServer::OnReadComplete, base::Unretained(this)));

  if (result == ERR_IO_PENDING) {
    synchronous_read_count_ = 0;
    return;
  }

  if (++synchronous_read_count_ > 32) {
    synchronous_read_count_ = 0;
    // Schedule the processing through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&QuicSimpleServer::OnReadComplete,
                              weak_factory_.GetWeakPtr(), result));
  } else {
    OnReadComplete(result);
  }
}

void QuicSimpleServer::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    LOG(ERROR) << "QuicSimpleServer read failed: " << ErrorToString(result);
    Shutdown();
    return;
  }

  QuicReceivedPacket packet(read_buffer_->data(), result,
                            helper_->GetClock()->Now(), false);
  dispatcher_->ProcessPacket(server_address_, client_address_, packet);

  StartReading();
}

}  // namespace net
