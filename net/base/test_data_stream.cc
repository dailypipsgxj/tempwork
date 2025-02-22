// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_data_stream.h"

namespace net {

TestDataStream::TestDataStream() {
  Reset();
}

// Fill |buffer| with |length| bytes of data from the stream.
void TestDataStream::GetBytes(char* buffer, int length) {
  while (length) {
    AdvanceIndex();
    int bytes_to_copy = std::min(length, bytes_remaining_);
    memcpy(buffer, buffer_ptr_, bytes_to_copy);
    buffer += bytes_to_copy;
    Consume(bytes_to_copy);
    length -= bytes_to_copy;
  }
}

bool TestDataStream::VerifyBytes(const char *buffer, int length) {
  while (length) {
    AdvanceIndex();
    int bytes_to_compare = std::min(length, bytes_remaining_);
    if (memcmp(buffer, buffer_ptr_, bytes_to_compare))
      return false;
    Consume(bytes_to_compare);
    length -= bytes_to_compare;
    buffer += bytes_to_compare;
  }
  return true;
}

void TestDataStream::Reset() {
  index_ = 0;
  bytes_remaining_ = 0;
  buffer_ptr_ = buffer_;
}

// If there is no data spilled over from the previous index, advance the
// index and fill the buffer.
void TestDataStream::AdvanceIndex() {
  if (bytes_remaining_ == 0) {
    // Convert it to ascii, but don't bother to reverse it.
    // (e.g. 12345 becomes "54321")
    int val = index_++;
    do {
      buffer_[bytes_remaining_++] = (val % 10) + '0';
    } while ((val /= 10) > 0);
    buffer_[bytes_remaining_++] = '.';
  }
}

// Consume data from the spill buffer.
void TestDataStream::Consume(int bytes) {
  bytes_remaining_ -= bytes;
  if (bytes_remaining_)
    buffer_ptr_ += bytes;
  else
    buffer_ptr_ = buffer_;
}

}  // namespace net

