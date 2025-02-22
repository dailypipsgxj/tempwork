// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An UpdateApplicator is used to iterate over a number of unapplied updates,
// applying them to the client using the given syncer session.
//
// UpdateApplicator might resemble an iterator, but it actually keeps retrying
// failed updates until no remaining updates can be successfully applied.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_APPLICATOR_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_APPLICATOR_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/sessions_impl/status_controller.h"
#include "components/sync/syncable/syncable_id.h"

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class WriteTransaction;
class Entry;
}

class ConflictResolver;
class Cryptographer;

class UpdateApplicator {
 public:
  explicit UpdateApplicator(Cryptographer* cryptographer);
  ~UpdateApplicator();

  // Attempt to apply the specified updates.
  void AttemptApplications(syncable::WriteTransaction* trans,
                           const std::vector<int64_t>& handles);

  int updates_applied() { return updates_applied_; }

  int encryption_conflicts() { return encryption_conflicts_; }

  int hierarchy_conflicts() { return hierarchy_conflicts_; }

  const std::set<syncable::Id>& simple_conflict_ids() {
    return simple_conflict_ids_;
  }

 private:
  // If true, AttemptOneApplication will skip over |entry| and return true.
  bool SkipUpdate(const syncable::Entry& entry);

  // Used to decrypt sensitive sync nodes.
  Cryptographer* cryptographer_;

  int updates_applied_;
  int encryption_conflicts_;
  int hierarchy_conflicts_;
  std::set<syncable::Id> simple_conflict_ids_;

  DISALLOW_COPY_AND_ASSIGN(UpdateApplicator);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_APPLICATOR_H_
