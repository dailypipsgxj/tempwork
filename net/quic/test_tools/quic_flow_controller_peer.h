// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_FLOW_CONTROLLER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_FLOW_CONTROLLER_PEER_H_

#include "base/macros.h"
#include "net/quic/core/quic_protocol.h"

namespace net {

class QuicFlowController;

namespace test {

class QuicFlowControllerPeer {
 public:
  static void SetSendWindowOffset(QuicFlowController* flow_controller,
                                  QuicStreamOffset offset);

  static void SetReceiveWindowOffset(QuicFlowController* flow_controller,
                                     QuicStreamOffset offset);

  static void SetMaxReceiveWindow(QuicFlowController* flow_controller,
                                  QuicByteCount window_size);

  static QuicStreamOffset SendWindowOffset(QuicFlowController* flow_controller);

  static QuicByteCount SendWindowSize(QuicFlowController* flow_controller);

  static QuicStreamOffset ReceiveWindowOffset(
      QuicFlowController* flow_controller);

  static QuicByteCount ReceiveWindowSize(QuicFlowController* flow_controller);

  static QuicByteCount WindowUpdateThreshold(
      QuicFlowController* flow_controller);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicFlowControllerPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_FLOW_CONTROLLER_PEER_H_
