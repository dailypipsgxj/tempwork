// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/shell/public/interfaces/service_manager.mojom.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"
#include "services/shell/tests/util.h"

namespace shell {

namespace {

const char kTestAppName[] = "mojo:lifecycle_unittest_app";
const char kTestParentName[] = "mojo:lifecycle_unittest_parent";
const char kTestExeName[] = "exe:lifecycle_unittest_exe";
const char kTestPackageName[] = "mojo:lifecycle_unittest_package";
const char kTestPackageAppNameA[] = "mojo:lifecycle_unittest_package_app_a";
const char kTestPackageAppNameB[] = "mojo:lifecycle_unittest_package_app_b";
const char kTestName[] = "mojo:lifecycle_unittest";

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

void DecrementCountAndQuitWhenZero(base::RunLoop* loop, size_t* count) {
  if (!--(*count))
    loop->Quit();
}

struct Instance {
  Instance() : pid(0) {}
  Instance(const Identity& identity, uint32_t pid)
      : identity(identity), pid(pid) {}

  Identity identity;
  uint32_t pid;
};

class InstanceState : public mojom::ServiceManagerListener {
 public:
  InstanceState(mojom::ServiceManagerListenerRequest request,
                base::RunLoop* loop)
      : binding_(this, std::move(request)), loop_(loop) {}
  ~InstanceState() override {}

  bool HasInstanceForName(const std::string& name) const {
    return instances_.find(name) != instances_.end();
  }
  size_t GetNewInstanceCount() const {
    return instances_.size() - initial_instances_.size();
  }
  void WaitForInstanceDestruction(base::RunLoop* loop) {
    DCHECK(!destruction_loop_);
    destruction_loop_ = loop;
    // First of all check to see if we should be spinning this loop at all -
    // the app(s) we're waiting on quitting may already have quit.
    TryToQuitDestructionLoop();
  }

 private:
  // mojom::ServiceManagerListener:
  void OnInit(mojo::Array<mojom::ServiceInfoPtr> instances) override {
    for (const auto& instance : instances) {
      Instance i(instance->identity.To<Identity>(), instance->pid);
      initial_instances_[i.identity.name()] = i;
      instances_[i.identity.name()] = i;
    }
    loop_->Quit();
  }
  void OnServiceCreated(mojom::ServiceInfoPtr instance) override {
    instances_[instance->identity->name] =
        Instance(instance->identity.To<Identity>(), instance->pid);
  }
  void OnServiceStarted(mojom::IdentityPtr identity_ptr,
                        uint32_t pid) override {
    Identity identity = identity_ptr.To<Identity>();
    for (auto& instance : instances_) {
      if (instance.second.identity == identity) {
        instance.second.pid = pid;
        break;
      }
    }
  }
  void OnServiceStopped(mojom::IdentityPtr identity_ptr) override {
    Identity identity = identity_ptr.To<Identity>();
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      if (it->second.identity == identity) {
        instances_.erase(it);
        break;
      }
    }
    TryToQuitDestructionLoop();
  }

  void TryToQuitDestructionLoop() {
    if (!GetNewInstanceCount() && destruction_loop_) {
      destruction_loop_->Quit();
      destruction_loop_ = nullptr;
    }
  }

  // All currently running instances.
  std::map<std::string, Instance> instances_;
  // The initial set of instances.
  std::map<std::string, Instance> initial_instances_;

  mojo::Binding<mojom::ServiceManagerListener> binding_;
  base::RunLoop* loop_;

  // Set when the client wants to wait for this object to track the destruction
  // of an instance before proceeding.
  base::RunLoop* destruction_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InstanceState);
};

}  // namespace

class LifecycleTest : public test::ServiceTest {
 public:
  LifecycleTest() : ServiceTest(kTestName) {}
  ~LifecycleTest() override {}

 protected:
  // test::ServiceTest:
  void SetUp() override {
    test::ServiceTest::SetUp();
    InitPackage();
    instances_ = TrackInstances();
  }
  void TearDown() override {
    instances_.reset();
    test::ServiceTest::TearDown();
  }

  bool CanRunCrashTest() {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch("single-process");
  }

  void InitPackage() {
    test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageName);
    base::RunLoop loop;
    lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
    lifecycle->GracefulQuit();
    loop.Run();
  }

  test::mojom::LifecycleControlPtr ConnectTo(const std::string& name) {
    test::mojom::LifecycleControlPtr lifecycle;
    connector()->ConnectToInterface(name, &lifecycle);
    PingPong(lifecycle.get());
    return lifecycle;
  }

  base::Process LaunchProcess() {
    base::Process process;
    test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
        "lifecycle_unittest_exe.exe",
#else
        "lifecycle_unittest_exe",
#endif
        Identity(kTestExeName, mojom::kInheritUserID),
        connector(),
        &process);
    return process;
  }

  void PingPong(test::mojom::LifecycleControl* lifecycle) {
    base::RunLoop loop;
    lifecycle->Ping(base::Bind(&QuitLoop, &loop));
    loop.Run();
  }

  InstanceState* instances() { return instances_.get(); }

  void WaitForInstanceDestruction() {
    base::RunLoop loop;
    instances()->WaitForInstanceDestruction(&loop);
    loop.Run();
  }

 private:
  std::unique_ptr<InstanceState> TrackInstances() {
    mojom::ServiceManagerPtr service_manager;
    connector()->ConnectToInterface("mojo:shell", &service_manager);
    mojom::ServiceManagerListenerPtr listener;
    base::RunLoop loop;
    InstanceState* state = new InstanceState(GetProxy(&listener), &loop);
    service_manager->AddListener(std::move(listener));
    loop.Run();
    return base::WrapUnique(state);
  }

  std::unique_ptr<InstanceState> instances_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleTest);
};

TEST_F(LifecycleTest, Standalone_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_CloseShellConnection) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->CloseShellConnection();

  WaitForInstanceDestruction();

  // |lifecycle| pipe should still be valid.
  PingPong(lifecycle.get());
}

TEST_F(LifecycleTest, PackagedApp_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, PackagedApp_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_CrashCrashesOtherProvidedApp) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  EXPECT_EQ(3u, instance_count);

  base::RunLoop loop;
  lifecycle_a.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_b.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_package.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));

  // Now crash one of the packaged apps.
  lifecycle_a->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_GracefulQuitPackageQuitsAll) {
  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  EXPECT_EQ(3u, instance_count);

  base::RunLoop loop;
  lifecycle_a.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_b.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_package.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));

  // Now quit the package. All the packaged apps should close.
  lifecycle_package->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Exe_GracefulQuit) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());

  process.Terminate(9, true);
}

TEST_F(LifecycleTest, Exe_TerminateProcess) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  process.Terminate(9, true);
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, ShutdownTree) {
  // Verifies that Instances are destroyed when their creator is.
  std::unique_ptr<Connection> parent_connection =
      connector()->Connect(kTestParentName);
  test::mojom::ParentPtr parent;
  parent_connection->GetInterface(&parent);

  // This asks kTestParentName to open a connection to kTestAppName and blocks
  // on a response from a Ping().
  {
    base::RunLoop loop;
    parent->ConnectToChild(base::Bind(&QuitLoop, &loop));
    loop.Run();
  }

  // Should now have two new instances (parent and child).
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());
  EXPECT_TRUE(instances()->HasInstanceForName(kTestParentName));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));

  parent->Quit();

  // Quitting the parent should cascade-quit the child.
  WaitForInstanceDestruction();
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
  EXPECT_FALSE(instances()->HasInstanceForName(kTestParentName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
}

}  // namespace shell
