// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_PORT_REF_H_
#define MOJO_EDK_SYSTEM_PORTS_PORT_REF_H_

#include "base/memory/ref_counted.h"
#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {
namespace ports {

class Port;
class Node;

class PortRef {
 public:
  ~PortRef();
  PortRef();
  PortRef(const PortName& name, const scoped_refptr<Port>& port);

  PortRef(const PortRef& other);
  PortRef& operator=(const PortRef& other);

  const PortName& name() const { return name_; }

 private:
  friend class Node;
  Port* port() const { return port_.get(); }

  PortName name_;
  scoped_refptr<Port> port_;
};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_PORT_REF_H_
