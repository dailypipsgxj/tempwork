// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_HTTP_BRIDGE_NETWORK_RESOURCES_H_
#define COMPONENTS_SYNC_CORE_HTTP_BRIDGE_NETWORK_RESOURCES_H_

#include <memory>

#include "components/sync/base/sync_export.h"
#include "components/sync/core/network_resources.h"
#include "components/sync/core/network_time_update_callback.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

class CancelationSignal;
class HttpPostProviderFactory;

class SYNC_EXPORT HttpBridgeNetworkResources : public NetworkResources {
 public:
  ~HttpBridgeNetworkResources() override;

  // NetworkResources
  std::unique_ptr<HttpPostProviderFactory> GetHttpPostProviderFactory(
      const scoped_refptr<net::URLRequestContextGetter>&
          baseline_context_getter,
      const NetworkTimeUpdateCallback& network_time_update_callback,
      CancelationSignal* cancelation_signal) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_HTTP_BRIDGE_NETWORK_RESOURCES_H_
