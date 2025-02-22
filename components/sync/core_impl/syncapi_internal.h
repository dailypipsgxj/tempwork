// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_INTERNAL_H_
#define COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_INTERNAL_H_

// The functions defined are shared among some of the classes that implement
// the internal sync API.  They are not to be used by clients of the API.

#include <string>

#include "components/sync/base/sync_export.h"

namespace sync_pb {
class AttachmentMetadata;
class EntitySpecifics;
class PasswordSpecificsData;
}

namespace syncer {

class Cryptographer;

sync_pb::PasswordSpecificsData* DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics,
    Cryptographer* crypto);

SYNC_EXPORT void SyncAPINameToServerName(const std::string& syncer_name,
                                         std::string* out);
SYNC_EXPORT void ServerNameToSyncAPIName(const std::string& server_name,
                                         std::string* out);

bool IsNameServerIllegalAfterTrimming(const std::string& name);

bool AreSpecificsEqual(const Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right);

// Return true iff |left| and |right| are equal.
bool AreAttachmentMetadataEqual(const sync_pb::AttachmentMetadata& left,
                                const sync_pb::AttachmentMetadata& right);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_INTERNAL_H_
