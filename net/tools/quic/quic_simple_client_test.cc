// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client.h"

#include "base/strings/string_util.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::CryptoTestUtils;

namespace net {
namespace test {

TEST(QuicSimpleClientTest, Initialize) {
  IPEndPoint server_address(IPEndPoint(net::test::Loopback4(), 80));
  QuicServerId server_id("hostname", server_address.port(),
                         PRIVACY_MODE_DISABLED);
  QuicVersionVector versions = QuicSupportedVersions();
  QuicSimpleClient client(server_address, server_id, versions,
                          CryptoTestUtils::ProofVerifierForTesting());
  EXPECT_TRUE(client.Initialize());
}

}  // namespace test
}  // namespace net
