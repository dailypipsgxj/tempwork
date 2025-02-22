// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_GLES2_CONTEXT_H_
#define SERVICES_UI_PUBLIC_CPP_GLES2_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "services/ui/public/interfaces/command_buffer.mojom.h"

namespace gpu {
class CommandBufferProxyImpl;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
}
}

namespace ui {

class CommandBufferClientImpl;

class GLES2Context {
 public:
  ~GLES2Context();
  gpu::gles2::GLES2Interface* interface() const {
    return implementation_.get();
  }
  gpu::ContextSupport* context_support() const { return implementation_.get(); }

  static std::unique_ptr<GLES2Context> CreateOffscreenContext(
      const std::vector<int32_t>& attribs);

 private:
  GLES2Context();
  bool Initialize(const std::vector<int32_t>& attribs);

  std::unique_ptr<CommandBufferClientImpl> command_buffer_client_impl_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_proxy_impl_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> implementation_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Context);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_GLES2_CONTEXT_H_
