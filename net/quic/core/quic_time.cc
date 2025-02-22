// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_time.h"

#include <limits>

#include "base/logging.h"

namespace net {

uint64_t QuicWallTime::ToUNIXSeconds() const {
  return microseconds_ / 1000000;
}

uint64_t QuicWallTime::ToUNIXMicroseconds() const {
  return microseconds_;
}

bool QuicWallTime::IsAfter(QuicWallTime other) const {
  return microseconds_ > other.microseconds_;
}

bool QuicWallTime::IsBefore(QuicWallTime other) const {
  return microseconds_ < other.microseconds_;
}

bool QuicWallTime::IsZero() const {
  return microseconds_ == 0;
}

QuicTime::Delta QuicWallTime::AbsoluteDifference(QuicWallTime other) const {
  uint64_t d;

  if (microseconds_ > other.microseconds_) {
    d = microseconds_ - other.microseconds_;
  } else {
    d = other.microseconds_ - microseconds_;
  }

  if (d > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    d = std::numeric_limits<int64_t>::max();
  }
  return QuicTime::Delta::FromMicroseconds(d);
}

QuicWallTime QuicWallTime::Add(QuicTime::Delta delta) const {
  uint64_t microseconds = microseconds_ + delta.ToMicroseconds();
  if (microseconds < microseconds_) {
    microseconds = std::numeric_limits<uint64_t>::max();
  }
  return QuicWallTime(microseconds);
}

// TODO(ianswett) Test this.
QuicWallTime QuicWallTime::Subtract(QuicTime::Delta delta) const {
  uint64_t microseconds = microseconds_ - delta.ToMicroseconds();
  if (microseconds > microseconds_) {
    microseconds = 0;
  }
  return QuicWallTime(microseconds);
}

}  // namespace net
