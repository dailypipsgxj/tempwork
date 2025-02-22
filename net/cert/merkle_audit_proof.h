// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MERKLE_AUDIT_PROOF_H_
#define NET_CERT_MERKLE_AUDIT_PROOF_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {
namespace ct {

// Returns the length of the audit path for a leaf at |leaf_index| in a Merkle
// tree containing |tree_size| leaves.
// The |leaf_index| must be less than the |tree_size|.
NET_EXPORT uint64_t CalculateAuditPathLength(uint64_t leaf_index,
                                             uint64_t tree_size);

// Audit proof for a Merkle tree leaf, as defined in section 2.1.1. of RFC6962.
struct NET_EXPORT MerkleAuditProof {
  MerkleAuditProof();
  MerkleAuditProof(uint64_t leaf_index,
                   const std::vector<std::string>& audit_path);
  ~MerkleAuditProof();

  // Index of the tree leaf in the log.
  uint64_t leaf_index = 0;

  // Audit path nodes.
  std::vector<std::string> nodes;
};

}  // namespace ct
}  // namespace net

#endif  // NET_CERT_MERKLE_AUDIT_PROOF_H_
