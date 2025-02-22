// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/mojo_application_info.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace shell {
class Connection;
class Connector;
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

class ConnectionFilter;

// Encapsulates a connection to a //services/shell.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add shell::Service implementations whose exposed interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// ContentBrowserClient::RegisterInProcessMojoApplications().
class CONTENT_EXPORT MojoShellConnection {
 public:
  using ServiceRequestHandler =
      base::Callback<void(shell::mojom::ServiceRequest)>;
  using Factory = base::Callback<std::unique_ptr<MojoShellConnection>(void)>;

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(std::unique_ptr<MojoShellConnection> connection);

  // Returns the per-process instance, or nullptr if the Shell connection has
  // not yet been bound. Must be called on the thread the connection was created
  // on.
  static MojoShellConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  virtual ~MojoShellConnection();

  // Sets the factory used to create the MojoShellConnection. This must be
  // called before the MojoShellConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Creates a MojoShellConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<MojoShellConnection> Create(
      shell::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections. Connection filters MUST be added
  // before calling this in order to avoid races. See AddConnectionFilter()
  // below.
  virtual void Start() = 0;

  // Sets a closure to be invoked once the connection receives an Initialize()
  // request from the shell.
  virtual void SetInitializeHandler(const base::Closure& handler) = 0;

  // Returns the shell::Connector received via this connection's Service
  // implementation. Use this to initiate connections as this object's Identity.
  virtual shell::Connector* GetConnector() = 0;

  // Returns this connection's identity with the shell. Connections initiated
  // via the shell::Connector returned by GetConnector() will use this.
  virtual const shell::Identity& GetIdentity() const = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  // Provides an InterfaceRegistry to forward incoming interface requests to
  // on the MojoShellConnection's own thread if they aren't bound by the
  // connection's internal InterfaceRegistry on the IO thread.
  //
  // Also configures |interface_provider| to forward all of its outgoing
  // interface requests to the connection's internal remote interface provider.
  //
  // Note that neither |interface_registry| or |interface_provider| is owned
  // and both MUST outlive the MojoShellConnection.
  //
  // TODO(rockot): Remove this. It's a temporary solution to avoid porting all
  // relevant code to ConnectionFilters at once.
  virtual void SetupInterfaceRequestProxies(
      shell::InterfaceRegistry* registry,
      shell::InterfaceProvider* provider) = 0;

  // Allows the caller to filter inbound connections and/or expose interfaces
  // on them. |filter| may be created on any thread, but will be used and
  // destroyed exclusively on the IO thread (the thread corresponding to
  // |io_task_runner| passed to Create() above.)
  //
  // Connection filters MUST be added before calling Start() in order to avoid
  // races.
  virtual void AddConnectionFilter(
      std::unique_ptr<ConnectionFilter> filter) = 0;

  // Returns ownership of |filter|, added via AddConnectionFilter(), to the
  // caller.
  virtual std::unique_ptr<ConnectionFilter> RemoveConnectionFilter(
      ConnectionFilter* filter) = 0;

  // Adds an embedded service to this connection's ServiceFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(const std::string& name,
                                  const MojoApplicationInfo& info) = 0;

  // Adds a generic ServiceRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateService() which
  // reference the given name.
  //
  // For in-process services, it is preferable to use |AddEmbeddedService()| as
  // defined above.
  virtual void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
