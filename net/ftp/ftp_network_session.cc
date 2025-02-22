// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_session.h"

namespace net {

FtpNetworkSession::FtpNetworkSession(HostResolver* host_resolver)
    : host_resolver_(host_resolver) {}

FtpNetworkSession::~FtpNetworkSession() {}

}  // namespace net

