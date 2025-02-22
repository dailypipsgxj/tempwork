// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/directory_commit_contribution.h"

#include <stdint.h>

#include <set>
#include <string>

#include "base/message_loop/message_loop.h"
#include "components/sync/base/attachment_id_proto.h"
#include "components/sync/sessions_impl/status_controller.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "components/sync/test/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class DirectoryCommitContributionTest : public ::testing::Test {
 public:
  void SetUp() override {
    dir_maker_.SetUp();

    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    CreateTypeRoot(&trans, dir(), PREFERENCES);
    CreateTypeRoot(&trans, dir(), EXTENSIONS);
    CreateTypeRoot(&trans, dir(), ARTICLES);
    CreateTypeRoot(&trans, dir(), BOOKMARKS);
  }

  void TearDown() override { dir_maker_.TearDown(); }

 protected:
  int64_t CreateUnsyncedItemWithAttachments(
      syncable::WriteTransaction* trans,
      ModelType type,
      const std::string& tag,
      const sync_pb::AttachmentMetadata& attachment_metadata) {
    // For bookmarks specify the Bookmarks root folder as the parent;
    // for other types leave the parent ID empty
    syncable::Id parent_id;
    if (type == BOOKMARKS) {
      syncable::Entry parent_entry(trans, syncable::GET_TYPE_ROOT, type);
      parent_id = parent_entry.GetId();
    }

    syncable::MutableEntry entry(trans, syncable::CREATE, type, parent_id, tag);
    if (attachment_metadata.record_size() > 0) {
      entry.PutAttachmentMetadata(attachment_metadata);
    }
    entry.PutIsUnsynced(true);
    return entry.GetMetahandle();
  }

  int64_t CreateUnsyncedItem(syncable::WriteTransaction* trans,
                             ModelType type,
                             const std::string& tag) {
    return CreateUnsyncedItemWithAttachments(trans, type, tag,
                                             sync_pb::AttachmentMetadata());
  }

  int64_t CreateSyncedItem(syncable::WriteTransaction* trans,
                           ModelType type,
                           const std::string& tag) {
    syncable::Entry parent_entry(trans, syncable::GET_TYPE_ROOT, type);
    syncable::MutableEntry entry(trans, syncable::CREATE, type,
                                 parent_entry.GetId(), tag);

    entry.PutId(syncable::Id::CreateFromServerId(
        id_factory_.NewServerId().GetServerId()));
    entry.PutBaseVersion(10);
    entry.PutServerVersion(10);
    entry.PutIsUnappliedUpdate(false);
    entry.PutIsUnsynced(false);
    entry.PutIsDel(false);
    entry.PutServerIsDel(false);

    return entry.GetMetahandle();
  }

  void CreateSuccessfulCommitResponse(
      const sync_pb::SyncEntity& entity,
      sync_pb::CommitResponse::EntryResponse* response) {
    response->set_response_type(sync_pb::CommitResponse::SUCCESS);
    response->set_non_unique_name(entity.name());
    response->set_version(entity.version() + 1);
    response->set_parent_id_string(entity.parent_id_string());

    if (entity.id_string()[0] == '-')  // Look for the - in 'c-1234' style IDs.
      response->set_id_string(id_factory_.NewServerId().GetServerId());
    else
      response->set_id_string(entity.id_string());
  }

  syncable::Directory* dir() { return dir_maker_.directory(); }

  TestIdFactory id_factory_;

  // Used in construction of DirectoryTypeDebugInfoEmitters.
  base::ObserverList<TypeDebugInfoObserver> type_observers_;

 private:
  base::MessageLoop loop_;  // Neeed to initialize the directory.
  TestDirectorySetterUpper dir_maker_;
};

// Verify that the DirectoryCommitContribution contains only entries of its
// specified type.
TEST_F(DirectoryCommitContributionTest, GatherByTypes) {
  int64_t pref1;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    pref1 = CreateUnsyncedItem(&trans, PREFERENCES, "pref1");
    CreateUnsyncedItem(&trans, PREFERENCES, "pref2");
    CreateUnsyncedItem(&trans, EXTENSIONS, "extension1");
  }

  DirectoryTypeDebugInfoEmitter emitter(PREFERENCES, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 5, &emitter));
  ASSERT_EQ(2U, cc->GetNumEntries());

  const std::vector<int64_t>& metahandles = cc->metahandles_;
  EXPECT_TRUE(std::find(metahandles.begin(), metahandles.end(), pref1) !=
              metahandles.end());
  EXPECT_TRUE(std::find(metahandles.begin(), metahandles.end(), pref1) !=
              metahandles.end());

  cc->CleanUp();
}

// Verify that the DirectoryCommitContributionTest builder function
// truncates if necessary.
TEST_F(DirectoryCommitContributionTest, GatherAndTruncate) {
  int64_t pref1;
  int64_t pref2;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    pref1 = CreateUnsyncedItem(&trans, PREFERENCES, "pref1");
    pref2 = CreateUnsyncedItem(&trans, PREFERENCES, "pref2");
    CreateUnsyncedItem(&trans, EXTENSIONS, "extension1");
  }

  DirectoryTypeDebugInfoEmitter emitter(PREFERENCES, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 1, &emitter));
  ASSERT_EQ(1U, cc->GetNumEntries());

  int64_t only_metahandle = cc->metahandles_[0];
  EXPECT_TRUE(only_metahandle == pref1 || only_metahandle == pref2);

  cc->CleanUp();
}

// Sanity check for building commits from DirectoryCommitContributions.
// This test makes two CommitContribution objects of different types and uses
// them to initialize a commit message.  Then it checks that the contents of the
// commit message match those of the directory they came from.
TEST_F(DirectoryCommitContributionTest, PrepareCommit) {
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    CreateUnsyncedItem(&trans, PREFERENCES, "pref1");
    CreateUnsyncedItem(&trans, PREFERENCES, "pref2");
    CreateUnsyncedItem(&trans, EXTENSIONS, "extension1");
  }

  DirectoryTypeDebugInfoEmitter emitter1(PREFERENCES, &type_observers_);
  DirectoryTypeDebugInfoEmitter emitter2(EXTENSIONS, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> pref_cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 25, &emitter1));
  std::unique_ptr<DirectoryCommitContribution> ext_cc(
      DirectoryCommitContribution::Build(dir(), EXTENSIONS, 25, &emitter2));

  sync_pb::ClientToServerMessage message;
  pref_cc->AddToCommitMessage(&message);
  ext_cc->AddToCommitMessage(&message);

  const sync_pb::CommitMessage& commit_message = message.commit();

  std::set<syncable::Id> ids_for_commit;
  ASSERT_EQ(3, commit_message.entries_size());
  for (int i = 0; i < commit_message.entries_size(); ++i) {
    const sync_pb::SyncEntity& entity = commit_message.entries(i);
    // The entities in this test have client-style IDs since they've never been
    // committed before, so we must use CreateFromClientString to re-create them
    // from the commit message.
    ids_for_commit.insert(
        syncable::Id::CreateFromClientString(entity.id_string()));
  }

  ASSERT_EQ(3U, ids_for_commit.size());
  {
    syncable::ReadTransaction trans(FROM_HERE, dir());
    for (std::set<syncable::Id>::iterator it = ids_for_commit.begin();
         it != ids_for_commit.end(); ++it) {
      SCOPED_TRACE(it->value());
      syncable::Entry entry(&trans, syncable::GET_BY_ID, *it);
      ASSERT_TRUE(entry.good());
      EXPECT_TRUE(entry.GetSyncing());
      EXPECT_FALSE(entry.GetDirtySync());
    }
  }

  pref_cc->CleanUp();
  ext_cc->CleanUp();
}

// Check that deletion requests include a model type.
// This was not always the case, but was implemented to allow us to loosen some
// other restrictions in the protocol.
TEST_F(DirectoryCommitContributionTest, DeletedItemsWithSpecifics) {
  int64_t pref1;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    pref1 = CreateSyncedItem(&trans, PREFERENCES, "pref1");
    syncable::MutableEntry e1(&trans, syncable::GET_BY_HANDLE, pref1);
    e1.PutIsDel(true);
    e1.PutIsUnsynced(true);
  }

  DirectoryTypeDebugInfoEmitter emitter(PREFERENCES, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> pref_cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 25, &emitter));
  ASSERT_TRUE(pref_cc);

  sync_pb::ClientToServerMessage message;
  pref_cc->AddToCommitMessage(&message);

  const sync_pb::CommitMessage& commit_message = message.commit();
  ASSERT_EQ(1, commit_message.entries_size());
  EXPECT_TRUE(commit_message.entries(0).specifics().has_preference());

  pref_cc->CleanUp();
}

// As ususal, bookmarks are special.  Bookmark deletion is special.
// Deleted bookmarks include a valid "is folder" bit and their full specifics
// (especially the meta info, which is what server really wants).
TEST_F(DirectoryCommitContributionTest, DeletedBookmarksWithSpecifics) {
  int64_t bm1;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    bm1 = CreateSyncedItem(&trans, BOOKMARKS, "bm1");
    syncable::MutableEntry e1(&trans, syncable::GET_BY_HANDLE, bm1);

    e1.PutIsDir(true);
    e1.PutServerIsDir(true);

    sync_pb::EntitySpecifics specifics;
    sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
    bm_specifics->set_url("http://www.chrome.com");
    bm_specifics->set_title("Chrome");
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key("K");
    meta_info->set_value("V");
    e1.PutSpecifics(specifics);

    e1.PutIsDel(true);
    e1.PutIsUnsynced(true);
  }

  DirectoryTypeDebugInfoEmitter emitter(BOOKMARKS, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> bm_cc(
      DirectoryCommitContribution::Build(dir(), BOOKMARKS, 25, &emitter));
  ASSERT_TRUE(bm_cc);

  sync_pb::ClientToServerMessage message;
  bm_cc->AddToCommitMessage(&message);

  const sync_pb::CommitMessage& commit_message = message.commit();
  ASSERT_EQ(1, commit_message.entries_size());

  const sync_pb::SyncEntity& entity = commit_message.entries(0);
  EXPECT_TRUE(entity.has_folder());
  ASSERT_TRUE(entity.specifics().has_bookmark());
  ASSERT_EQ(1, entity.specifics().bookmark().meta_info_size());
  EXPECT_EQ("K", entity.specifics().bookmark().meta_info(0).key());
  EXPECT_EQ("V", entity.specifics().bookmark().meta_info(0).value());

  bm_cc->CleanUp();
}

// Test that bookmarks support hierarchy.
TEST_F(DirectoryCommitContributionTest, HierarchySupport_Bookmark) {
  // Create a normal-looking bookmark item.
  int64_t bm1;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    bm1 = CreateSyncedItem(&trans, BOOKMARKS, "bm1");
    syncable::MutableEntry e(&trans, syncable::GET_BY_HANDLE, bm1);

    sync_pb::EntitySpecifics specifics;
    sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
    bm_specifics->set_url("http://www.chrome.com");
    bm_specifics->set_title("Chrome");
    e.PutSpecifics(specifics);

    e.PutIsDel(false);
    e.PutIsUnsynced(true);

    EXPECT_TRUE(e.ShouldMaintainHierarchy());
  }

  DirectoryTypeDebugInfoEmitter emitter(BOOKMARKS, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> bm_cc(
      DirectoryCommitContribution::Build(dir(), BOOKMARKS, 25, &emitter));

  sync_pb::ClientToServerMessage message;
  bm_cc->AddToCommitMessage(&message);
  const sync_pb::CommitMessage& commit_message = message.commit();
  bm_cc->CleanUp();

  ASSERT_EQ(1, commit_message.entries_size());
  EXPECT_TRUE(commit_message.entries(0).has_parent_id_string());
  EXPECT_FALSE(commit_message.entries(0).parent_id_string().empty());
}

// Test that preferences do not support hierarchy.
TEST_F(DirectoryCommitContributionTest, HierarchySupport_Preferences) {
  // Create a normal-looking prefs item.
  int64_t pref1;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    pref1 = CreateUnsyncedItem(&trans, PREFERENCES, "pref1");
    syncable::MutableEntry e(&trans, syncable::GET_BY_HANDLE, pref1);

    EXPECT_FALSE(e.ShouldMaintainHierarchy());
  }

  DirectoryTypeDebugInfoEmitter emitter(PREFERENCES, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> pref_cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 25, &emitter));

  sync_pb::ClientToServerMessage message;
  pref_cc->AddToCommitMessage(&message);
  const sync_pb::CommitMessage& commit_message = message.commit();
  pref_cc->CleanUp();

  ASSERT_EQ(1, commit_message.entries_size());
  EXPECT_FALSE(commit_message.entries(0).has_parent_id_string());
  EXPECT_TRUE(commit_message.entries(0).parent_id_string().empty());
}

void AddAttachment(sync_pb::AttachmentMetadata* metadata, bool is_on_server) {
  sync_pb::AttachmentMetadataRecord record;
  *record.mutable_id() = CreateAttachmentIdProto(0, 0);
  record.set_is_on_server(is_on_server);
  *metadata->add_record() = record;
}

// Creates some unsynced items, pretends to commit them, and hands back a
// specially crafted response to the syncer in order to test commit response
// processing.  The response simulates a succesful commit scenario.
TEST_F(DirectoryCommitContributionTest, ProcessCommitResponse) {
  int64_t pref1_handle;
  int64_t pref2_handle;
  int64_t ext1_handle;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());
    pref1_handle = CreateUnsyncedItem(&trans, PREFERENCES, "pref1");
    pref2_handle = CreateUnsyncedItem(&trans, PREFERENCES, "pref2");
    ext1_handle = CreateUnsyncedItem(&trans, EXTENSIONS, "extension1");
  }

  DirectoryTypeDebugInfoEmitter emitter1(PREFERENCES, &type_observers_);
  DirectoryTypeDebugInfoEmitter emitter2(EXTENSIONS, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> pref_cc(
      DirectoryCommitContribution::Build(dir(), PREFERENCES, 25, &emitter1));
  std::unique_ptr<DirectoryCommitContribution> ext_cc(
      DirectoryCommitContribution::Build(dir(), EXTENSIONS, 25, &emitter2));

  sync_pb::ClientToServerMessage message;
  pref_cc->AddToCommitMessage(&message);
  ext_cc->AddToCommitMessage(&message);

  const sync_pb::CommitMessage& commit_message = message.commit();
  ASSERT_EQ(3, commit_message.entries_size());

  sync_pb::ClientToServerResponse response;
  for (int i = 0; i < commit_message.entries_size(); ++i) {
    sync_pb::SyncEntity entity = commit_message.entries(i);
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response.mutable_commit()->add_entryresponse();
    CreateSuccessfulCommitResponse(entity, entry_response);
  }

  sessions::StatusController status;

  // Process these in reverse order.  Just because we can.
  ext_cc->ProcessCommitResponse(response, &status);
  pref_cc->ProcessCommitResponse(response, &status);

  {
    syncable::ReadTransaction trans(FROM_HERE, dir());
    syncable::Entry p1(&trans, syncable::GET_BY_HANDLE, pref1_handle);
    EXPECT_TRUE(p1.GetId().ServerKnows());
    EXPECT_FALSE(p1.GetSyncing());
    EXPECT_FALSE(p1.GetDirtySync());
    EXPECT_LT(0, p1.GetServerVersion());

    syncable::Entry p2(&trans, syncable::GET_BY_HANDLE, pref2_handle);
    EXPECT_TRUE(p2.GetId().ServerKnows());
    EXPECT_FALSE(p2.GetSyncing());
    EXPECT_FALSE(p2.GetDirtySync());
    EXPECT_LT(0, p2.GetServerVersion());

    syncable::Entry e1(&trans, syncable::GET_BY_HANDLE, ext1_handle);
    EXPECT_TRUE(e1.GetId().ServerKnows());
    EXPECT_FALSE(e1.GetSyncing());
    EXPECT_FALSE(e1.GetDirtySync());
    EXPECT_LT(0, e1.GetServerVersion());
  }

  pref_cc->CleanUp();
  ext_cc->CleanUp();
}

// Creates some unsynced items with attachments and verifies that only items
// where all attachments have been uploaded to the server are eligible to be
// committed.
TEST_F(DirectoryCommitContributionTest, ProcessCommitResponseWithAttachments) {
  int64_t art1_handle;
  int64_t art2_handle;
  int64_t art3_handle;
  {
    syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir());

    // art1 has two attachments, both have been uploaded to the server.  art1 is
    // eligible to be committed.
    sync_pb::AttachmentMetadata art1_attachments;
    AddAttachment(&art1_attachments, true /* is_on_server */);
    AddAttachment(&art1_attachments, true /* is_on_server */);
    art1_handle = CreateUnsyncedItemWithAttachments(&trans, ARTICLES, "art1",
                                                    art1_attachments);

    // art2 has two attachments, one of which has been uploaded to the
    // server. art2 is not eligible to be committed.
    sync_pb::AttachmentMetadata art2_attachments;
    AddAttachment(&art2_attachments, false /* is_on_server */);
    AddAttachment(&art2_attachments, true /* is_on_server */);
    art2_handle = CreateUnsyncedItemWithAttachments(&trans, ARTICLES, "art2",
                                                    art2_attachments);

    // art3 has two attachments, neither of which have been uploaded to the
    // server. art2 is not eligible to be committed.
    sync_pb::AttachmentMetadata art3_attachments;
    AddAttachment(&art3_attachments, false /* is_on_server */);
    AddAttachment(&art3_attachments, false /* is_on_server */);
    art3_handle = CreateUnsyncedItemWithAttachments(&trans, ARTICLES, "art3",
                                                    art3_attachments);
  }

  DirectoryTypeDebugInfoEmitter emitter(ARTICLES, &type_observers_);
  std::unique_ptr<DirectoryCommitContribution> art_cc(
      DirectoryCommitContribution::Build(dir(), ARTICLES, 25, &emitter));

  // Only art1 is ready.
  EXPECT_EQ(1U, art_cc->GetNumEntries());

  sync_pb::ClientToServerMessage message;
  art_cc->AddToCommitMessage(&message);

  const sync_pb::CommitMessage& commit_message = message.commit();
  ASSERT_EQ(1, commit_message.entries_size());

  sync_pb::ClientToServerResponse response;
  for (int i = 0; i < commit_message.entries_size(); ++i) {
    sync_pb::SyncEntity entity = commit_message.entries(i);
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response.mutable_commit()->add_entryresponse();
    CreateSuccessfulCommitResponse(entity, entry_response);
  }

  sessions::StatusController status;
  art_cc->ProcessCommitResponse(response, &status);
  {
    syncable::ReadTransaction trans(FROM_HERE, dir());

    syncable::Entry a1(&trans, syncable::GET_BY_HANDLE, art1_handle);
    EXPECT_TRUE(a1.GetId().ServerKnows());
    EXPECT_FALSE(a1.GetSyncing());
    EXPECT_FALSE(a1.GetDirtySync());
    EXPECT_LT(0, a1.GetServerVersion());

    syncable::Entry a2(&trans, syncable::GET_BY_HANDLE, art2_handle);
    EXPECT_FALSE(a2.GetId().ServerKnows());
    EXPECT_FALSE(a2.GetSyncing());
    EXPECT_FALSE(a2.GetDirtySync());
    EXPECT_EQ(0, a2.GetServerVersion());

    syncable::Entry a3(&trans, syncable::GET_BY_HANDLE, art3_handle);
    EXPECT_FALSE(a3.GetId().ServerKnows());
    EXPECT_FALSE(a3.GetSyncing());
    EXPECT_FALSE(a3.GetDirtySync());
    EXPECT_EQ(0, a3.GetServerVersion());
  }

  art_cc->CleanUp();
}

}  // namespace syncer
