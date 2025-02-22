// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_getter.h"

#include "base/debug/leak_annotations.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter_observer.h"

namespace net {

void URLRequestContextGetter::AddObserver(
    URLRequestContextGetterObserver* observer) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  observer_list_.AddObserver(observer);
}

void URLRequestContextGetter::RemoveObserver(
    URLRequestContextGetterObserver* observer) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  observer_list_.RemoveObserver(observer);
}

URLRequestContextGetter::URLRequestContextGetter() {}

URLRequestContextGetter::~URLRequestContextGetter() {}

void URLRequestContextGetter::OnDestruct() const {
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      GetNetworkTaskRunner();
  DCHECK(network_task_runner.get());
  if (network_task_runner.get()) {
    if (network_task_runner->BelongsToCurrentThread()) {
      delete this;
    } else {
      if (!network_task_runner->DeleteSoon(FROM_HERE, this)) {
        // Can't force-delete the object here, because some derived classes
        // can only be deleted on the owning thread, so just emit a warning to
        // aid in debugging.
        DLOG(WARNING) << "URLRequestContextGetter leaking due to no owning"
                      << " thread.";
        // Let LSan know we know this is a leak. https://crbug.com/594130
        ANNOTATE_LEAKING_OBJECT_PTR(this);
      }
    }
  }
  // If no IO task runner was available, we will just leak memory.
  // This is also true if the IO thread is gone.
}

void URLRequestContextGetter::NotifyContextShuttingDown() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  // Once shutdown starts, this must always return NULL.
  DCHECK(!GetURLRequestContext());

  FOR_EACH_OBSERVER(URLRequestContextGetterObserver, observer_list_,
                    OnContextShuttingDown());
}

TrivialURLRequestContextGetter::TrivialURLRequestContextGetter(
    URLRequestContext* context,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : context_(context), main_task_runner_(main_task_runner) {
}

TrivialURLRequestContextGetter::~TrivialURLRequestContextGetter() {}

URLRequestContext* TrivialURLRequestContextGetter::GetURLRequestContext() {
  return context_;
}

scoped_refptr<base::SingleThreadTaskRunner>
TrivialURLRequestContextGetter::GetNetworkTaskRunner() const {
  return main_task_runner_;
}


}  // namespace net
