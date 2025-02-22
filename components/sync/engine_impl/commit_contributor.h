// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_CONTRIBUTOR_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_CONTRIBUTOR_H_

#include <cstddef>
#include <memory>

#include "components/sync/base/sync_export.h"

namespace syncer {

class CommitContribution;

namespace syncable {
class Directory;
}

// This class represents a source of items to commit to the sync server.
//
// When asked, it can return CommitContribution objects that contain a set of
// items to be committed from this source.
class SYNC_EXPORT CommitContributor {
 public:
  CommitContributor();
  virtual ~CommitContributor() = 0;

  // Gathers up to |max_entries| unsynced items from this contributor into a
  // CommitContribution.  Returns NULL when the contributor has nothing to
  // contribute.
  virtual std::unique_ptr<CommitContribution> GetContribution(
      size_t max_entries) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_CONTRIBUTOR_H_
