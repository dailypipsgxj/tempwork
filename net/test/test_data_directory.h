// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_DATA_DIRECTORY_H_
#define NET_TEST_TEST_DATA_DIRECTORY_H_

#include "base/files/file_path.h"

namespace net {

// Returns the FilePath object representing the absolute path in the source
// tree that contains certificates for testing.
base::FilePath GetTestCertsDirectory();

// Returns the base::FilePath object representing the path to client
// certificate files to be used in the |client_authorities| list
// of a net::SSLConfig object. For all other uses, use
// GetTestCertsDirectory() instead.
base::FilePath GetTestClientCertsDirectory();

// Returns the base::FilePath object representing the relative path containing
// resource files for testing WebSocket. Typically the FilePath will be used as
// document root argument for net::SpawnedTestServer with TYPE_WS or TYPE_WSS.
base::FilePath GetWebSocketTestDataDirectory();

}  // namespace net

#endif  // NET_TEST_TEST_DATA_DIRECTORY_H_
