// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/router_test_util.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

void AllocRequestMessage(uint32_t name, const char* text, Message* message) {
  size_t payload_size = strlen(text) + 1;  // Plus null terminator.
  internal::RequestMessageBuilder builder(name, payload_size);
  memcpy(builder.buffer()->Allocate(payload_size), text, payload_size);

  builder.message()->MoveTo(message);
}

void AllocResponseMessage(uint32_t name,
                          const char* text,
                          uint64_t request_id,
                          Message* message) {
  size_t payload_size = strlen(text) + 1;  // Plus null terminator.
  internal::ResponseMessageBuilder builder(name, payload_size, request_id);
  memcpy(builder.buffer()->Allocate(payload_size), text, payload_size);

  builder.message()->MoveTo(message);
}

MessageAccumulator::MessageAccumulator(MessageQueue* queue,
                                       const base::Closure& closure)
    : queue_(queue), closure_(closure) {}

MessageAccumulator::~MessageAccumulator() {}

bool MessageAccumulator::Accept(Message* message) {
  queue_->Push(message);
  if (!closure_.is_null()) {
    closure_.Run();
    closure_.Reset();
  }
  return true;
}

ResponseGenerator::ResponseGenerator() {}

bool ResponseGenerator::Accept(Message* message) {
  return false;
}

bool ResponseGenerator::AcceptWithResponder(
    Message* message,
    MessageReceiverWithStatus* responder) {
  EXPECT_TRUE(message->has_flag(Message::kFlagExpectsResponse));

  bool result = SendResponse(message->name(), message->request_id(),
                             reinterpret_cast<const char*>(message->payload()),
                             responder);
  EXPECT_TRUE(responder->IsValid());
  delete responder;
  return result;
}

bool ResponseGenerator::SendResponse(uint32_t name,
                                     uint64_t request_id,
                                     const char* request_string,
                                     MessageReceiver* responder) {
  Message response;
  std::string response_string(request_string);
  response_string += " world!";
  AllocResponseMessage(name, response_string.c_str(), request_id, &response);

  return responder->Accept(&response);
}

LazyResponseGenerator::LazyResponseGenerator(const base::Closure& closure)
    : responder_(nullptr), name_(0), request_id_(0), closure_(closure) {}

LazyResponseGenerator::~LazyResponseGenerator() {
  delete responder_;
}

bool LazyResponseGenerator::AcceptWithResponder(
    Message* message,
    MessageReceiverWithStatus* responder) {
  name_ = message->name();
  request_id_ = message->request_id();
  request_string_ =
      std::string(reinterpret_cast<const char*>(message->payload()));
  responder_ = responder;
  if (!closure_.is_null()) {
    closure_.Run();
    closure_.Reset();
  }
  return true;
}

void LazyResponseGenerator::Complete(bool send_response) {
  if (send_response) {
    SendResponse(name_, request_id_, request_string_.c_str(), responder_);
  }
  delete responder_;
  responder_ = nullptr;
}

}  // namespace test
}  // namespace mojo
