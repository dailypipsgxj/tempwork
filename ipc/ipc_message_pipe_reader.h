// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_PIPE_READER_H_
#define IPC_IPC_MESSAGE_PIPE_READER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
namespace internal {

class AsyncHandleWaiter;

// A helper class to handle bytestream directly over mojo::MessagePipe
// in template-method pattern. MessagePipeReader manages the lifetime
// of given MessagePipe and participates the event loop, and
// read the stream and call the client when it is ready.
//
// Each client has to:
//
//  * Provide a subclass implemenation of a specific use of a MessagePipe
//    and implement callbacks.
//  * Create the subclass instance with a MessagePipeHandle.
//    The constructor automatically start listening on the pipe.
//
// All functions must be called on the IO thread, except for Send(), which can
// be called on any thread. All |Delegate| functions will be called on the IO
// thread.
//
class IPC_EXPORT MessagePipeReader : public NON_EXPORTED_BASE(mojom::Channel) {
 public:
  class Delegate {
   public:
    virtual void OnPeerPidReceived() = 0;
    virtual void OnMessageReceived(const Message& message) = 0;
    virtual void OnPipeError() = 0;
    virtual void OnAssociatedInterfaceRequest(
        const std::string& name,
        mojo::ScopedInterfaceEndpointHandle handle) = 0;
  };

  // Builds a reader that reads messages from |receive_handle| and lets
  // |delegate| know.
  //
  // |pipe| is the message pipe handle corresponding to the channel's master
  // interface. This is the message pipe underlying both |sender| and
  // |receiver|.
  //
  // Both |sender| and |receiver| must be non-null.
  //
  // Note that MessagePipeReader doesn't delete |delegate|.
  MessagePipeReader(mojo::MessagePipeHandle pipe,
                    mojom::ChannelAssociatedPtr sender,
                    mojo::AssociatedInterfaceRequest<mojom::Channel> receiver,
                    Delegate* delegate);
  ~MessagePipeReader() override;

  // Close and destroy the MessagePipe.
  void Close();

  // Return true if the MessagePipe is alive.
  bool IsValid() { return sender_; }

  // Sends an IPC::Message to the other end of the pipe. Safe to call from any
  // thread.
  bool Send(std::unique_ptr<Message> message);

  // Requests an associated interface from the other end of the pipe.
  void GetRemoteInterface(const std::string& name,
                          mojo::ScopedInterfaceEndpointHandle handle);

  base::ProcessId GetPeerPid() const { return peer_pid_; }

 protected:
  void OnPipeClosed();
  void OnPipeError(MojoResult error);

 private:
  // mojom::Channel:
  void SetPeerPid(int32_t peer_pid) override;
  void Receive(mojo::Array<uint8_t> data,
               mojo::Array<mojom::SerializedHandlePtr> handles) override;
  void GetAssociatedInterface(
      const mojo::String& name,
      mojom::GenericInterfaceAssociatedRequest request) override;

  // |delegate_| is null once the message pipe is closed.
  Delegate* delegate_;
  base::ProcessId peer_pid_ = base::kNullProcessId;
  mojom::ChannelAssociatedPtr sender_;
  mojo::AssociatedBinding<mojom::Channel> binding_;

  // Raw message pipe handle and interface ID we use to send legacy IPC messages
  // over the associated pipe.
  const uint32_t sender_interface_id_;
  const mojo::MessagePipeHandle sender_pipe_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_PIPE_READER_H_
