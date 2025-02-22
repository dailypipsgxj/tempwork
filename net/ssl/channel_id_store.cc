// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/channel_id_store.h"

#include <utility>

#include "crypto/ec_private_key.h"

namespace net {

ChannelIDStore::ChannelID::ChannelID() {
}

ChannelIDStore::ChannelID::ChannelID(const std::string& server_identifier,
                                     base::Time creation_time,
                                     std::unique_ptr<crypto::ECPrivateKey> key)
    : server_identifier_(server_identifier),
      creation_time_(creation_time),
      key_(std::move(key)) {}

ChannelIDStore::ChannelID::ChannelID(const ChannelID& other)
    : server_identifier_(other.server_identifier_),
      creation_time_(other.creation_time_),
      key_(other.key_ ? other.key_->Copy() : nullptr) {
}

ChannelIDStore::ChannelID& ChannelIDStore::ChannelID::operator=(
    const ChannelID& other) {
  if (&other == this)
    return *this;
  server_identifier_ = other.server_identifier_;
  creation_time_ = other.creation_time_;
  if (other.key_)
    key_ = other.key_->Copy();
  return *this;
}

ChannelIDStore::ChannelID::~ChannelID() {}

void ChannelIDStore::InitializeFrom(const ChannelIDList& list) {
  for (ChannelIDList::const_iterator i = list.begin(); i != list.end();
      ++i) {
    SetChannelID(std::unique_ptr<ChannelID>(new ChannelID(*i)));
  }
}

}  // namespace net
