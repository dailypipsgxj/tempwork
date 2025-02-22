// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_WAITER_TEST_UTILS_H_
#define MOJO_EDK_SYSTEM_WAITER_TEST_UTILS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/simple_thread.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/waiter.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {
namespace test {

// This is a very simple thread that has a |Waiter|, on which it waits
// indefinitely (and records the result). It will create and initialize the
// |Waiter| on creation, but the caller must start the thread with |Start()|. It
// will join the thread on destruction.
//
// One usually uses it like:
//
//    MojoResult result;
//    {
//      AwakableList awakable_list;
//      test::SimpleWaiterThread thread(&result);
//      awakable_list.Add(thread.waiter(), ...);
//      thread.Start();
//      ... some stuff to wake the waiter ...
//      awakable_list.Remove(thread.waiter());
//    }  // Join |thread|.
//    EXPECT_EQ(..., result);
//
// There's a bit of unrealism in its use: In this sort of usage, calls such as
// |Waiter::Init()|, |AddAwakable()|, and |RemoveAwakable()| are done in the
// main (test) thread, not the waiter thread (as would actually happen in real
// code). (We accept this unrealism for simplicity, since |AwakableList| is
// thread-unsafe so making it more realistic would require adding nontrivial
// synchronization machinery.)
class SimpleWaiterThread : public base::SimpleThread {
 public:
  // For the duration of the lifetime of this object, |*result| belongs to it
  // (in the sense that it will write to it whenever it wants).
  SimpleWaiterThread(MojoResult* result, uintptr_t* context);
  ~SimpleWaiterThread() override;  // Joins the thread.

  Waiter* waiter() { return &waiter_; }

 private:
  void Run() override;

  MojoResult* const result_;
  uintptr_t* const context_;
  Waiter waiter_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWaiterThread);
};

// This is a more complex and realistic thread that has a |Waiter|, on which it
// waits for the given deadline (with the given flags). Unlike
// |SimpleWaiterThread|, it requires the machinery of |Dispatcher|.
class WaiterThread : public base::SimpleThread {
 public:
  // Note: |*did_wait_out|, |*result_out|, |*context_out| and
  // |*signals_state_out| "belong" to this object (i.e., may be modified by, on
  // some other thread) while it's alive.
  WaiterThread(scoped_refptr<Dispatcher> dispatcher,
               MojoHandleSignals handle_signals,
               MojoDeadline deadline,
               uintptr_t context,
               bool* did_wait_out,
               MojoResult* result_out,
               uintptr_t* context_out,
               HandleSignalsState* signals_state_out);
  ~WaiterThread() override;

 private:
  void Run() override;

  const scoped_refptr<Dispatcher> dispatcher_;
  const MojoHandleSignals handle_signals_;
  const MojoDeadline deadline_;
  const uint32_t context_;
  bool* const did_wait_out_;
  MojoResult* const result_out_;
  uintptr_t* const context_out_;
  HandleSignalsState* const signals_state_out_;

  Waiter waiter_;

  DISALLOW_COPY_AND_ASSIGN(WaiterThread);
};

}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_WAITER_TEST_UTILS_H_
