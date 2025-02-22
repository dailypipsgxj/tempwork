// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/signed_certificate_timestamp_and_status.h"

#include "net/cert/signed_certificate_timestamp.h"

namespace net {

SignedCertificateTimestampAndStatus::SignedCertificateTimestampAndStatus() {}

SignedCertificateTimestampAndStatus::SignedCertificateTimestampAndStatus(
    const scoped_refptr<ct::SignedCertificateTimestamp>& sct,
    const ct::SCTVerifyStatus status)
    : sct(sct), status(status) {}

SignedCertificateTimestampAndStatus::SignedCertificateTimestampAndStatus(
    const SignedCertificateTimestampAndStatus& other) = default;

SignedCertificateTimestampAndStatus::~SignedCertificateTimestampAndStatus() {}

}  // namespace net
