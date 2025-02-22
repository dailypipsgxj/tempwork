// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_MESSAGE_QUEUE_H_
#define MOJO_EDK_SYSTEM_PORTS_MESSAGE_QUEUE_H_

#include <stdint.h>

#include <deque>
#include <functional>
#include <limits>
#include <vector>

#include "base/macros.h"
#include "mojo/edk/system/ports/message.h"

namespace mojo {
namespace edk {
namespace ports {

const uint64_t kInitialSequenceNum = 1;
const uint64_t kInvalidSequenceNum = std::numeric_limits<uint64_t>::max();

// An incoming message queue for a port. MessageQueue keeps track of the highest
// known sequence number and can indicate whether the next sequential message is
// available. Thus the queue enforces message ordering for the consumer without
// enforcing it for the producer (see AcceptMessage() below.)
class MessageQueue {
 public:
  explicit MessageQueue();
  explicit MessageQueue(uint64_t next_sequence_num);
  ~MessageQueue();

  void set_signalable(bool value) { signalable_ = value; }

  uint64_t next_sequence_num() const { return next_sequence_num_; }

  bool HasNextMessage() const;

  // Gives ownership of the message. The selector may be null.
  void GetNextMessageIf(std::function<bool(const Message&)> selector,
                        ScopedMessage* message);

  // Takes ownership of the message. Note: Messages are ordered, so while we
  // have added a message to the queue, we may still be waiting on a message
  // ahead of this one before we can let any of the messages be returned by
  // GetNextMessage.
  //
  // Furthermore, once has_next_message is set to true, it will remain false
  // until GetNextMessage is called enough times to return a null message.
  // In other words, has_next_message acts like an edge trigger.
  //
  void AcceptMessage(ScopedMessage message, bool* has_next_message);

  // Returns all of the ports referenced by messages in this message queue.
  void GetReferencedPorts(std::deque<PortName>* ports);

 private:
  std::vector<ScopedMessage> heap_;
  uint64_t next_sequence_num_;
  bool signalable_ = true;

  DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_MESSAGE_QUEUE_H_
