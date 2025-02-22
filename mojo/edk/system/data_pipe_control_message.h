// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_CONTROL_MESSAGE_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_CONTROL_MESSAGE_H_

#include <stdint.h>

#include <memory>

#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/ports/port_ref.h"
#include "mojo/public/c/system/macros.h"

namespace mojo {
namespace edk {

class NodeController;
class PortsMessage;

enum DataPipeCommand : uint32_t {
  // Signal to the consumer that new data is available.
  DATA_WAS_WRITTEN,

  // Signal to the producer that data has been consumed.
  DATA_WAS_READ,
};

// Message header for messages sent over a data pipe control port.
struct MOJO_ALIGNAS(8) DataPipeControlMessage {
  DataPipeCommand command;
  uint32_t num_bytes;
};

void SendDataPipeControlMessage(NodeController* node_controller,
                                const ports::PortRef& port,
                                DataPipeCommand command,
                                uint32_t num_bytes);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_CONTROL_MESSAGE_H_
