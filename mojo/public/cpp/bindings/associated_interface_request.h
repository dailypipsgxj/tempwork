// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// AssociatedInterfaceRequest represents an associated interface request. It is
// similar to InterfaceRequest except that it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfaceRequest {
 public:
  // Constructs an empty AssociatedInterfaceRequest, representing that the
  // client is not requesting an implementation of Interface.
  AssociatedInterfaceRequest() {}
  AssociatedInterfaceRequest(decltype(nullptr)) {}

  // Takes the interface endpoint handle from another
  // AssociatedInterfaceRequest.
  AssociatedInterfaceRequest(AssociatedInterfaceRequest&& other) {
    handle_ = std::move(other.handle_);
  }
  AssociatedInterfaceRequest& operator=(AssociatedInterfaceRequest&& other) {
    if (this != &other)
      handle_ = std::move(other.handle_);
    return *this;
  }

  // Assigning to nullptr resets the AssociatedInterfaceRequest to an empty
  // state, closing the interface endpoint handle currently bound to it (if
  // any).
  AssociatedInterfaceRequest& operator=(decltype(nullptr)) {
    handle_.reset();
    return *this;
  }

  // Indicates whether the request currently contains a valid interface endpoint
  // handle.
  bool is_pending() const { return handle_.is_valid(); }

  void Bind(ScopedInterfaceEndpointHandle handle) {
    handle_ = std::move(handle);
  }

  ScopedInterfaceEndpointHandle PassHandle() {
    return std::move(handle_);
  }

  const ScopedInterfaceEndpointHandle& handle() const { return handle_; }

  bool Equals(const AssociatedInterfaceRequest& other) const {
    if (this == &other)
      return true;

    // Now that the two refer to different objects, they are equivalent if
    // and only if they are both invalid.
    return !is_pending() && !other.is_pending();
  }

 private:
  ScopedInterfaceEndpointHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfaceRequest);
};

// Makes an AssociatedInterfaceRequest bound to the specified associated
// endpoint.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeAssociatedRequest(
    ScopedInterfaceEndpointHandle handle) {
  AssociatedInterfaceRequest<Interface> request;
  request.Bind(std::move(handle));
  return request;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_REQUEST_H_
