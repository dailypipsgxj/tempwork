// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_DELEGATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_DELEGATE_H_

#include "mojo/public/cpp/bindings/interface_id.h"

namespace mojo {

class PipeControlMessageHandlerDelegate {
 public:
  // The implementation of the following methods should return false if the
  // notification is unexpected. In that case, the user of this delegate is
  // expected to close the message pipe.
  virtual bool OnPeerAssociatedEndpointClosed(InterfaceId id) = 0;
  virtual bool OnAssociatedEndpointClosedBeforeSent(InterfaceId id) = 0;

 protected:
  virtual ~PipeControlMessageHandlerDelegate() {}
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_HANDLER_DELEGATE_H_
