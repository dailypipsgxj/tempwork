// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"

namespace safe_browsing {

class V4Store;

typedef base::Callback<void(std::unique_ptr<V4Store>)>
    UpdatedStoreReadyCallback;

// The size of the hash prefix, in bytes. It should be between 4 to 32 (full
// hash).
typedef size_t PrefixSize;

// A hash prefix sent by the SafeBrowsing PVer4 service.
typedef std::string HashPrefix;

// The sorted list of hash prefixes.
typedef std::string HashPrefixes;

// Stores the list of sorted hash prefixes, by size.
// For instance: {4: ["abcd", "bcde", "cdef", "gggg"], 5: ["fffff"]}
typedef base::hash_map<PrefixSize, HashPrefixes> HashPrefixMap;

// Stores the iterator to the last element merged from the HashPrefixMap for a
// given prefix size.
// For instance: {4:iter(3), 5:iter(1)} means that we have already merged
// 3 hash prefixes of length 4, and 1 hash prefix of length 5.
typedef base::hash_map<PrefixSize, HashPrefixes::const_iterator> IteratorMap;

// A full SHA256 hash.
typedef HashPrefix FullHash;

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes.  DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreReadResult {
  // No errors.
  READ_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_READ_FAILURE = 1,

  // The contents of the file could not be read.
  FILE_UNREADABLE_FAILURE = 2,

  // The file was found to be empty.
  FILE_EMPTY_FAILURE = 3,

  // The contents of the file could not be interpreted as a valid
  // V4StoreFileFormat proto.
  PROTO_PARSING_FAILURE = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  UNEXPECTED_MAGIC_NUMBER_FAILURE = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  FILE_VERSION_INCOMPATIBLE_FAILURE = 6,

  // The rest of the file could not be parsed as a ListUpdateResponse protobuf.
  // This can happen if the machine crashed before the file was fully written to
  // disk or if there was disk corruption.
  HASH_PREFIX_INFO_MISSING_FAILURE = 7,

  // Unable to generate the hash prefix map from the updates on disk.
  HASH_PREFIX_MAP_GENERATION_FAILURE = 8,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_READ_RESULT_MAX
};

// Enumerate different failure events while writing the file to disk after
// applying updates for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreWriteResult {
  // No errors.
  WRITE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_WRITE_FAILURE = 1,

  // The proto being written to disk wasn't a FULL_UPDATE proto.
  INVALID_RESPONSE_TYPE_FAILURE = 2,

  // Number of bytes written to disk was different from the size of the proto.
  UNEXPECTED_BYTES_WRITTEN_FAILURE = 3,

  // Renaming the temporary file to store file failed.
  UNABLE_TO_RENAME_FAILURE = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_WRITE_RESULT_MAX
};

// Enumerate different events while applying the update fetched fom the server
// for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum ApplyUpdateResult {
  // No errors.
  APPLY_UPDATE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_APPLY_UPDATE_FAILURE = 1,

  // Prefix size smaller than 4 (which is the lowest expected).
  PREFIX_SIZE_TOO_SMALL_FAILURE = 2,

  // Prefix size larger than 32 (length of a full SHA256 hash).
  PREFIX_SIZE_TOO_LARGE_FAILURE = 3,

  // The number of bytes in additions isn't a multiple of prefix size.
  ADDITIONS_SIZE_UNEXPECTED_FAILURE = 4,

  // The update received from the server contains a prefix that's already
  // present in the map.
  ADDITIONS_HAS_EXISTING_PREFIX_FAILURE = 5,

  // The server sent a response_type that the client did not expect.
  UNEXPECTED_RESPONSE_TYPE_FAILURE = 6,

  // One of more index(es) in removals field of the response is greater than
  // the number of hash prefixes currently in the (old) store.
  REMOVALS_INDEX_TOO_LARGE = 7,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  APPLY_UPDATE_RESULT_MAX
};

// Factory for creating V4Store. Tests implement this factory to create fake
// stores for testing.
class V4StoreFactory {
 public:
  virtual ~V4StoreFactory() {}
  virtual V4Store* CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path);
};

class V4Store {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path);
  virtual ~V4Store();

  const std::string& state() const { return state_; }

  const base::FilePath& store_path() const { return store_path_; }

  void ApplyUpdate(std::unique_ptr<ListUpdateResponse> response,
                   const scoped_refptr<base::SingleThreadTaskRunner>&,
                   UpdatedStoreReadyCallback);

  // If a hash prefix in this store matches |full_hash|, returns that hash
  // prefix; otherwise returns an empty hash prefix.
  virtual HashPrefix GetMatchingHashPrefix(const FullHash& full_hash);

  std::string DebugString() const;

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

  // Reset internal state and delete the backing file.
  virtual bool Reset();

 private:
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromEmptyFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromAbsentFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromInvalidContentsFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromUnexpectedMagicFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromLowVersionFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixInfoFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixesFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteNoResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWritePartialResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteFullResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromFileWithUnknownProto);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestAddUnlumpedHashesWithInvalidAddition);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAddUnlumpedHashes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAddUnlumpedHashesWithEmptyString);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestGetNextSmallestUnmergedPrefixWithEmptyPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestGetNextSmallestUnmergedPrefix);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesWithSameSizesInEachMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesWithDifferentSizesInEachMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesOldMapRunsOutFirst);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesAdditionsMapRunsOutFirst);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesFailsForRepeatedHashPrefix);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesFailsWhenRemovalsIndexTooLarge);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesOnlyElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesFirstElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesMiddleElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesLastElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesRemovesWhenOldHasDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesRemovesMultipleAcrossDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestReadFullResponseWithValidHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestReadFullResponseWithInvalidHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheBeginning);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsInTheMiddle);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheEnd);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsAtTheBeginningOfEven);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheEndOfEven);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixDoesNotExistInConcatenatedList);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestFullHashExistsInMapWithSingleSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestFullHashExistsInMapWithDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsInMapWithSingleSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsInMapWithDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixDoesNotExistInMapWithDifferentSizes);

  // If |prefix_size| is within expected range, and |raw_hashes| is not invalid,
  // then it sets |raw_hashes| as the value at key |prefix_size| in
  // |additions_map|
  static ApplyUpdateResult AddUnlumpedHashes(PrefixSize prefix_size,
                                             const std::string& raw_hashes,
                                             HashPrefixMap* additions_map);

  // Get the next unmerged hash prefix in dictionary order from
  // |hash_prefix_map|. |iterator_map| is used to determine which hash prefixes
  // have been merged already. Returns true if there are any unmerged hash
  // prefixes in the list.
  static bool GetNextSmallestUnmergedPrefix(
      const HashPrefixMap& hash_prefix_map,
      const IteratorMap& iterator_map,
      HashPrefix* smallest_hash_prefix);

  // Returns true if |hash_prefix| exists between |begin| and |end| iterators.
  static bool HashPrefixMatches(const HashPrefix& hash_prefix,
                                const HashPrefixes::const_iterator& begin,
                                const HashPrefixes::const_iterator& end);

  // For each key in |hash_prefix_map|, sets the iterator at that key
  // |iterator_map| to hash_prefix_map[key].begin().
  static void InitializeIteratorMap(const HashPrefixMap& hash_prefix_map,
                                    IteratorMap* iterator_map);

  // Reserve the appropriate string size so that the string size of the merged
  // list is exact. This ignores the space that would otherwise be released by
  // deletions specified in the update because it is non-trivial to calculate
  // those deletions upfront. This isn't so bad since deletions are supposed to
  // be small and infrequent.
  static void ReserveSpaceInPrefixMap(const HashPrefixMap& other_prefixes_map,
                                      HashPrefixMap* prefix_map_to_update);

  // Updates the |additions_map| with the additions received in the partial
  // update from the server.
  static ApplyUpdateResult UpdateHashPrefixMapFromAdditions(
      const ::google::protobuf::RepeatedPtrField<ThreatEntrySet>& additions,
      HashPrefixMap* additions_map);

  // Merges the prefix map from the old store (|old_hash_prefix_map|) and the
  // update (additions_map) to populate the prefix map for the current store.
  // The indices in the |raw_removals| list, which may be NULL, are not merged.
  ApplyUpdateResult MergeUpdate(const HashPrefixMap& old_hash_prefix_map,
                                const HashPrefixMap& additions_map,
                                const ::google::protobuf::RepeatedField<
                                    ::google::protobuf::int32>* raw_removals);

  // Reads the state of the store from the file on disk and returns the reason
  // for the failure or reports success.
  StoreReadResult ReadFromDisk();

  // Writes the FULL_UPDATE |response| to disk as a V4StoreFileFormat proto.
  StoreWriteResult WriteToDisk(
      std::unique_ptr<ListUpdateResponse> response) const;

  // The state of the store as returned by the PVer4 server in the last applied
  // update response.
  std::string state_;
  const base::FilePath store_path_;
  HashPrefixMap hash_prefix_map_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

std::ostream& operator<<(std::ostream& os, const V4Store& store);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_