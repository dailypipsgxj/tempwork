// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_
#define COMPONENTS_SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_

#include <string>

#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_export.h"

namespace syncer {

enum SyncProtocolErrorType {
  // Success case.
  SYNC_SUCCESS,

  // Birthday does not match that of the server.
  NOT_MY_BIRTHDAY,

  // Server is busy. Try later.
  THROTTLED,

  // Clear user data is being currently executed by the server.
  CLEAR_PENDING,

  // Server cannot service the request now.
  TRANSIENT_ERROR,

  // Indicates the datatypes have been migrated and the client should resync
  // them to get the latest progress markers.
  MIGRATION_DONE,

  // Invalid Credential.
  INVALID_CREDENTIAL,

  // An administrator disabled sync for this domain.
  DISABLED_BY_ADMIN,

  // Some of servers are busy. Try later with busy servers.
  PARTIAL_FAILURE,

  // Returned when server detects that this client's data is obsolete. Client
  // should reset local data and restart syncing.
  CLIENT_DATA_OBSOLETE,

  // The default value.
  UNKNOWN_ERROR
};

enum ClientAction {
  // Upgrade the client to latest version.
  UPGRADE_CLIENT,

  // Clear user data and setup sync again.
  CLEAR_USER_DATA_AND_RESYNC,

  // Set the bit on the account to enable sync.
  ENABLE_SYNC_ON_ACCOUNT,

  // Stop sync and restart sync.
  STOP_AND_RESTART_SYNC,

  // Wipe this client of any sync data.
  DISABLE_SYNC_ON_CLIENT,

  // Account is disabled by admin. Stop sync, clear prefs and show message on
  // settings page that account is disabled.
  STOP_SYNC_FOR_DISABLED_ACCOUNT,

  // Generated in response to CLIENT_DATA_OBSOLETE error. ProfileSyncService
  // should stop sync engine, delete directory and restart sync engine.
  RESET_LOCAL_SYNC_DATA,

  // The default. No action.
  UNKNOWN_ACTION
};

struct SYNC_EXPORT SyncProtocolError {
  SyncProtocolErrorType error_type;
  std::string error_description;
  std::string url;
  ClientAction action;
  ModelTypeSet error_data_types;
  SyncProtocolError();
  SyncProtocolError(const SyncProtocolError& other);
  ~SyncProtocolError();
  base::DictionaryValue* ToValue() const;
};

SYNC_EXPORT const char* GetSyncErrorTypeString(SyncProtocolErrorType type);
SYNC_EXPORT const char* GetClientActionString(ClientAction action);
}  // namespace syncer
#endif  // COMPONENTS_SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_
