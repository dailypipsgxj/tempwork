// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace test {

class MessageQueue;

void AllocRequestMessage(uint32_t name, const char* text, Message* message);
void AllocResponseMessage(uint32_t name,
                          const char* text,
                          uint64_t request_id,
                          Message* message);

class MessageAccumulator : public MessageReceiver {
 public:
  MessageAccumulator(MessageQueue* queue,
                     const base::Closure& closure = base::Closure());
  ~MessageAccumulator() override;

  bool Accept(Message* message) override;

 private:
  MessageQueue* queue_;
  base::Closure closure_;
};

class ResponseGenerator : public MessageReceiverWithResponderStatus {
 public:
  ResponseGenerator();

  bool Accept(Message* message) override;

  bool AcceptWithResponder(Message* message,
                           MessageReceiverWithStatus* responder) override;

  bool SendResponse(uint32_t name,
                    uint64_t request_id,
                    const char* request_string,
                    MessageReceiver* responder);
};

class LazyResponseGenerator : public ResponseGenerator {
 public:
  explicit LazyResponseGenerator(
      const base::Closure& closure = base::Closure());

  ~LazyResponseGenerator() override;

  bool AcceptWithResponder(Message* message,
                           MessageReceiverWithStatus* responder) override;

  bool has_responder() const { return !!responder_; }

  bool responder_is_valid() const { return responder_->IsValid(); }

  void set_closure(const base::Closure& closure) { closure_ = closure; }

  // Sends the response and delete the responder.
  void CompleteWithResponse() { Complete(true); }

  // Deletes the responder without sending a response.
  void CompleteWithoutResponse() { Complete(false); }

 private:
  // Completes the request handling by deleting responder_. Optionally
  // also sends a response.
  void Complete(bool send_response);

  MessageReceiverWithStatus* responder_;
  uint32_t name_;
  uint64_t request_id_;
  std::string request_string_;
  base::Closure closure_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_
