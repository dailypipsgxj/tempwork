// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a configure GetUpdates request to the server.
class SYNC_EXPORT ConfigureGetUpdatesRequestEvent : public ProtocolEvent {
 public:
  ConfigureGetUpdatesRequestEvent(
      base::Time timestamp,
      sync_pb::SyncEnums::GetUpdatesOrigin origin,
      const sync_pb::ClientToServerMessage& request);
  ~ConfigureGetUpdatesRequestEvent() override;

  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  std::unique_ptr<base::DictionaryValue> GetProtoMessage() const override;
  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  const base::Time timestamp_;
  const sync_pb::SyncEnums::GetUpdatesOrigin origin_;
  const sync_pb::ClientToServerMessage request_;

  DISALLOW_COPY_AND_ASSIGN(ConfigureGetUpdatesRequestEvent);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_CONFIGURE_GET_UPDATES_REQUEST_EVENT_H_
