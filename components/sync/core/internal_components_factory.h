// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// InternalComponentsFactory exists so that tests can override creation of
// components used by the SyncManager that are not exposed across the sync
// API boundary.

#ifndef COMPONENTS_SYNC_CORE_INTERNAL_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_CORE_INTERNAL_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

class CancelationSignal;
class ExtensionsActivity;
class ModelTypeRegistry;
class ServerConnectionManager;
class SyncEngineEventListener;
class SyncScheduler;

namespace sessions {
class DebugInfoGetter;
class SyncSessionContext;
}

namespace syncable {
class Directory;
class DirectoryBackingStore;
}

class SYNC_EXPORT InternalComponentsFactory {
 public:
  enum EncryptionMethod {
    ENCRYPTION_LEGACY,
    // Option to enable support for keystore key based encryption.
    ENCRYPTION_KEYSTORE
  };

  enum BackoffOverride {
    BACKOFF_NORMAL,
    // Use this value for integration testing to avoid long delays /
    // timing out tests. Uses kInitialBackoffShortRetrySeconds (see
    // polling_constants.h) for all initial retries.
    BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE
  };

  enum PreCommitUpdatesPolicy {
    // By default, the server will enable or disable this experiment through the
    // sync protocol's experiments data type.
    SERVER_CONTROLLED_PRE_COMMIT_UPDATE_AVOIANCE,

    // This flag overrides the server's decision and enables the pre-commit
    // update avoidance experiment.
    FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE,
  };

  // Configuration options for internal components. This struct is expected
  // to grow and shrink over time with transient features / experiments,
  // roughly following command line flags in chrome. Implementations of
  // InternalComponentsFactory can use this information to build components
  // with appropriate bells and whistles.
  struct Switches {
    EncryptionMethod encryption_method;
    BackoffOverride backoff_override;
    PreCommitUpdatesPolicy pre_commit_updates_policy;
  };

  // For selecting the types of storage to use to persist sync data when
  // BuildDirectoryBackingStore() is called.
  enum StorageOption {
    // BuildDirectoryBackingStore should not use persistent on-disk storage.
    STORAGE_IN_MEMORY,
    // Use this if you want BuildDirectoryBackingStore to create/use a real
    // on disk store.
    STORAGE_ON_DISK,
    // Use this to test the case where a directory fails to load.
    STORAGE_INVALID
  };

  virtual ~InternalComponentsFactory() {}

  virtual std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context,
      CancelationSignal* cancelation_signal) = 0;

  virtual std::unique_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id) = 0;

  virtual std::unique_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(StorageOption storage,
                             const std::string& dir_name,
                             const base::FilePath& backing_filepath) = 0;

  // Returns the Switches struct that this object is using as configuration, if
  // the implementation is making use of one.
  virtual Switches GetSwitches() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_INTERNAL_COMPONENTS_FACTORY_H_
