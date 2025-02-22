// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/tests/connect/connect_test.mojom.h"

namespace shell {

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestClassApp
    : public Service,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public InterfaceFactory<test::mojom::ClassInterface>,
      public test::mojom::ConnectTestService,
      public test::mojom::ClassInterface {
 public:
  ConnectTestClassApp() {}
  ~ConnectTestClassApp() override {}

 private:
  // shell::Service:
  void OnStart(const Identity& identity) override {
    identity_ = identity;
  }
  bool OnConnect(Connection* connection) override {
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::ClassInterface>(this);
    inbound_connections_.insert(connection);
    connection->SetConnectionLostClosure(
        base::Bind(&ConnectTestClassApp::OnConnectionError,
                   base::Unretained(this), connection));
    return true;
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(const Identity& remote_identity,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ClassInterface>:
  void Create(const Identity& remote_identity,
              test::mojom::ClassInterfaceRequest request) override {
    class_interface_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("CLASS APP");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  // test::mojom::ClassInterface:
  void Ping(const PingCallback& callback) override {
    callback.Run("PONG");
  }

  void OnConnectionError(Connection* connection) {
    auto it = inbound_connections_.find(connection);
    DCHECK(it != inbound_connections_.end());
    inbound_connections_.erase(it);
    if (inbound_connections_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Identity identity_;
  std::set<Connection*> inbound_connections_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::ClassInterface> class_interface_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestClassApp);
};

}  // namespace shell


MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new shell::ConnectTestClassApp);
  return runner.Run(service_request_handle);
}
