// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_handle_watcher.h"

#include "base/logging.h"

namespace mojo {

SyncHandleWatcher::SyncHandleWatcher(
    const Handle& handle,
    MojoHandleSignals handle_signals,
    const SyncHandleRegistry::HandleCallback& callback)
    : handle_(handle),
      handle_signals_(handle_signals),
      callback_(callback),
      registered_(false),
      register_request_count_(0),
      registry_(SyncHandleRegistry::current()),
      destroyed_(new base::RefCountedData<bool>(false)) {}

SyncHandleWatcher::~SyncHandleWatcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (registered_)
    registry_->UnregisterHandle(handle_);

  destroyed_->data = true;
}

void SyncHandleWatcher::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  IncrementRegisterCount();
}

bool SyncHandleWatcher::SyncWatch(const bool* should_stop) {
  DCHECK(thread_checker_.CalledOnValidThread());
  IncrementRegisterCount();
  if (!registered_) {
    DecrementRegisterCount();
    return false;
  }

  // This object may be destroyed during the WatchAllHandles() call. So we have
  // to preserve the boolean that WatchAllHandles uses.
  auto destroyed = destroyed_;
  const bool* should_stop_array[] = {should_stop, &destroyed->data};
  bool result = registry_->WatchAllHandles(should_stop_array, 2);

  // This object has been destroyed.
  if (destroyed->data)
    return false;

  DecrementRegisterCount();
  return result;
}

void SyncHandleWatcher::IncrementRegisterCount() {
  register_request_count_++;
  if (!registered_) {
    registered_ =
        registry_->RegisterHandle(handle_, handle_signals_, callback_);
  }
}

void SyncHandleWatcher::DecrementRegisterCount() {
  DCHECK_GT(register_request_count_, 0u);

  register_request_count_--;
  if (register_request_count_ == 0 && registered_) {
    registry_->UnregisterHandle(handle_);
    registered_ = false;
  }
}

}  // namespace mojo
