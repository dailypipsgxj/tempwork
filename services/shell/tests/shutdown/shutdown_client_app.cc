// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/tests/shutdown/shutdown_unittest.mojom.h"

namespace shell {

class ShutdownClientApp
    : public Service,
      public InterfaceFactory<mojom::ShutdownTestClientController>,
      public mojom::ShutdownTestClientController,
      public mojom::ShutdownTestClient {
 public:
  ShutdownClientApp() {}
  ~ShutdownClientApp() override {}

 private:
  // shell::Service:
  bool OnConnect(Connection* connection) override {
    connection->AddInterface<mojom::ShutdownTestClientController>(this);
    return true;
  }

  // InterfaceFactory<mojom::ShutdownTestClientController>:
  void Create(const Identity& remote_identity,
              mojom::ShutdownTestClientControllerRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShutdownTestClientController:
  void ConnectAndWait(const ConnectAndWaitCallback& callback) override {
    mojom::ShutdownTestServicePtr service;
    connector()->ConnectToInterface("mojo:shutdown_service",
                                               &service);

    mojo::Binding<mojom::ShutdownTestClient> client_binding(this);

    mojom::ShutdownTestClientPtr client_ptr =
        client_binding.CreateInterfacePtrAndBind();

    service->SetClient(std::move(client_ptr));

    base::MessageLoop::ScopedNestableTaskAllower nestable_allower(
        base::MessageLoop::current());
    base::RunLoop run_loop;
    client_binding.set_connection_error_handler(run_loop.QuitClosure());
    run_loop.Run();

    callback.Run();
  }

  mojo::BindingSet<mojom::ShutdownTestClientController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownClientApp);
};

}  // namespace shell


MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new shell::ShutdownClientApp);
  return runner.Run(service_request_handle);
}
