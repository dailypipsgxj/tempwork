// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_quic_client_promised_info.h"

using std::string;

namespace net {
namespace test {

MockQuicClientPromisedInfo::MockQuicClientPromisedInfo(
    QuicClientSessionBase* session,
    QuicStreamId id,
    string url)
    : QuicClientPromisedInfo(session, id, url) {}

MockQuicClientPromisedInfo::~MockQuicClientPromisedInfo() {}

}  // namespace test
}  // namespace net
