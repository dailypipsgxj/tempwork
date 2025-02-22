// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_WATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_WATCHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/sync_handle_registry.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// SyncHandleWatcher supports watching a handle synchronously. It also supports
// registering the handle with a thread-local storage (SyncHandleRegistry), so
// that when other SyncHandleWatcher instances on the same thread perform sync
// handle watching, this handle will be watched together.
//
// SyncHandleWatcher is used for sync methods. While a sync call is waiting for
// response, we would like to block the thread. On the other hand, we need
// incoming sync method requests on the same thread to be able to reenter. We
// also need master interface endpoints to continue dispatching messages for
// associated endpoints on different threads.
//
// This class is not thread safe.
class SyncHandleWatcher {
 public:
  // Note: |handle| must outlive this object.
  SyncHandleWatcher(const Handle& handle,
                    MojoHandleSignals handle_signals,
                    const SyncHandleRegistry::HandleCallback& callback);

  ~SyncHandleWatcher();

  // Registers |handle_| with SyncHandleRegistry, so that when others perform
  // sync handle watching on the same thread, |handle_| will be watched
  // together.
  void AllowWokenUpBySyncWatchOnSameThread();

  // Waits on |handle_| plus all handles registered with SyncHandleRegistry and
  // runs callbacks synchronously for those ready handles.
  // This method:
  //   - returns true when |should_stop| is set to true;
  //   - return false when any error occurs, including this object being
  //     destroyed during a callback.
  bool SyncWatch(const bool* should_stop);

 private:
  void IncrementRegisterCount();
  void DecrementRegisterCount();

  const Handle handle_;
  const MojoHandleSignals handle_signals_;
  SyncHandleRegistry::HandleCallback callback_;

  // Whether |handle_| has been registered with SyncHandleRegistry.
  bool registered_;
  // If non-zero, |handle_| should be registered with SyncHandleRegistry.
  size_t register_request_count_;

  scoped_refptr<SyncHandleRegistry> registry_;

  scoped_refptr<base::RefCountedData<bool>> destroyed_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SyncHandleWatcher);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_WATCHER_H_
