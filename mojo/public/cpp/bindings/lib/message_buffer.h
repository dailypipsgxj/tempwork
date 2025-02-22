// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_LIB_MESSAGE_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_LIB_MESSAGE_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/system/message.h"

namespace mojo {
namespace internal {

// A fixed-size Buffer implementation using a Mojo message object for storage.
class MessageBuffer : public Buffer {
 public:
  // Initializes this buffer to carry a fixed byte capacity and no handles.
  MessageBuffer(size_t capacity, bool zero_initialized);

  // Initializes this buffer from an existing Mojo MessageHandle.
  MessageBuffer(ScopedMessageHandle message, uint32_t num_bytes);

  ~MessageBuffer() override;

  void* data() const { return buffer_; }
  uint32_t data_num_bytes() const { return data_num_bytes_; }

  // Buffer:
  void* Allocate(size_t delta) override;

  ScopedMessageHandle TakeMessage() { return std::move(message_); }

  void NotifyBadMessage(const std::string& error);

 private:
  uint32_t data_num_bytes_ = 0;
  ScopedMessageHandle message_;
  void* buffer_;

  uint32_t bytes_claimed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageBuffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_LIB_MESSAGE_BUFFER_H_
