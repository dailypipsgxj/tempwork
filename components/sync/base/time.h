// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Time-related sync functions.

#ifndef COMPONENTS_SYNC_BASE_TIME_H_
#define COMPONENTS_SYNC_BASE_TIME_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "components/sync/base/sync_export.h"

namespace syncer {

// Converts a time object to the format used in sync protobufs (ms
// since the Unix epoch).
SYNC_EXPORT int64_t TimeToProtoTime(const base::Time& t);

// Converts a time field from sync protobufs to a time object.
SYNC_EXPORT base::Time ProtoTimeToTime(int64_t proto_t);

SYNC_EXPORT std::string GetTimeDebugString(const base::Time& t);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_TIME_H_
