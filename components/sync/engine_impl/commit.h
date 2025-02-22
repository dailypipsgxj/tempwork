// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/sessions_impl/nudge_tracker.h"

namespace syncer {

namespace sessions {
class StatusController;
class SyncSession;
}

class CommitProcessor;
class Syncer;

// This class wraps the actions related to building and executing a single
// commit operation.
//
// This class' most important responsibility is to manage the ContributionsMap.
// This class serves as a container for those objects.  Although it would have
// been acceptable to let this class be a dumb container object, it turns out
// that there was no other convenient place to put the Init() and
// PostAndProcessCommitResponse() functions.  So they ended up here.
class SYNC_EXPORT Commit {
 public:
  typedef std::map<ModelType, std::unique_ptr<CommitContribution>>
      ContributionMap;

  Commit(ContributionMap contributions,
         const sync_pb::ClientToServerMessage& message,
         ExtensionsActivity::Records extensions_activity_buffer);

  // This destructor will DCHECK if CleanUp() has not been called.
  ~Commit();

  static Commit* Init(ModelTypeSet requested_types,
                      ModelTypeSet enabled_types,
                      size_t max_entries,
                      const std::string& account_name,
                      const std::string& cache_guid,
                      bool cookie_jar_mismatch,
                      bool cookie_jar_empty,
                      CommitProcessor* commit_processor,
                      ExtensionsActivity* extensions_activity);

  SyncerError PostAndProcessResponse(sessions::NudgeTracker* nudge_tracker,
                                     sessions::SyncSession* session,
                                     sessions::StatusController* status,
                                     ExtensionsActivity* extensions_activity);

  // Cleans up state associated with this commit.  Must be called before the
  // destructor.
  void CleanUp();

 private:
  ContributionMap contributions_;

  sync_pb::ClientToServerMessage message_;
  sync_pb::ClientToServerResponse response_;
  ExtensionsActivity::Records extensions_activity_buffer_;

  // Debug only flag used to indicate if it's safe to destruct the object.
  bool cleaned_up_;

  DISALLOW_COPY_AND_ASSIGN(Commit);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_H_
