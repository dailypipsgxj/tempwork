// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/data_batch_impl.h"

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

TEST(DataBatchImplTest, PutAndNextWithReuse) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();

  DataBatchImpl batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());

  const TagAndData& pair1 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("one", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  batch.Put("two", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  const TagAndData& pair2 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("two", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());
}

TEST(DataBatchImplTest, PutAndNextInterleaved) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();
  EntityData* entity3 = new EntityData();

  DataBatchImpl batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("two", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  const TagAndData& pair1 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("one", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  batch.Put("three", base::WrapUnique(entity3));
  EXPECT_TRUE(batch.HasNext());

  const TagAndData& pair2 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("two", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());

  const TagAndData& pair3 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("three", pair3.first);
  EXPECT_EQ(entity3, pair3.second.get());
}

TEST(DataBatchImplTest, PutAndNextSharedTag) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();

  DataBatchImpl batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("same", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("same", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  const TagAndData& pair1 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("same", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  const TagAndData& pair2 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("same", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());
}

}  // namespace syncer_v2
