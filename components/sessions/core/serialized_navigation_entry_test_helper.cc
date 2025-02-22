// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry_test_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sync/base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sessions {

namespace test_data {

const int kIndex = 3;
const int kUniqueID = 50;
const GURL kReferrerURL = GURL("http://www.referrer.com");
const int kReferrerPolicy = 0;
const GURL kVirtualURL= GURL("http://www.virtual-url.com");
const base::string16 kTitle = base::ASCIIToUTF16("title");
const std::string kEncodedPageState = "page state";
const ui::PageTransition kTransitionType =
    ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_AUTO_SUBFRAME |
        ui::PAGE_TRANSITION_HOME_PAGE |
        ui::PAGE_TRANSITION_CLIENT_REDIRECT);
const bool kHasPostData = true;
const int64_t kPostID = 100;
const GURL kOriginalRequestURL = GURL("http://www.original-request.com");
const bool kIsOverridingUserAgent = true;
const base::Time kTimestamp = syncer::ProtoTimeToTime(100);
const base::string16 kSearchTerms = base::ASCIIToUTF16("my search terms");
const GURL kFaviconURL = GURL("http://virtual-url.com/favicon.ico");
const int kHttpStatusCode = 404;
const GURL kRedirectURL0 = GURL("http://go/redirect0");
const GURL kRedirectURL1 = GURL("http://go/redirect1");
const GURL kOtherURL = GURL("http://other.com");
const int kPageID = 10;

}  // namespace test_data

// static
void SerializedNavigationEntryTestHelper::ExpectNavigationEquals(
    const SerializedNavigationEntry& expected,
    const SerializedNavigationEntry& actual) {
  EXPECT_EQ(expected.referrer_url_, actual.referrer_url_);
  EXPECT_EQ(expected.referrer_policy_, actual.referrer_policy_);
  EXPECT_EQ(expected.virtual_url_, actual.virtual_url_);
  EXPECT_EQ(expected.title_, actual.title_);
  EXPECT_EQ(expected.encoded_page_state_, actual.encoded_page_state_);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      actual.transition_type_, expected.transition_type_));
  EXPECT_EQ(expected.has_post_data_, actual.has_post_data_);
  EXPECT_EQ(expected.original_request_url_, actual.original_request_url_);
  EXPECT_EQ(expected.is_overriding_user_agent_,
            actual.is_overriding_user_agent_);
}

// static
SerializedNavigationEntry SerializedNavigationEntryTestHelper::CreateNavigation(
    const std::string& virtual_url,
    const std::string& title) {
  SerializedNavigationEntry navigation;
  navigation.index_ = 0;
  navigation.referrer_url_ = GURL("http://www.referrer.com");
  navigation.virtual_url_ = GURL(virtual_url);
  navigation.title_ = base::UTF8ToUTF16(title);
  navigation.encoded_page_state_ = "fake state";
  navigation.timestamp_ = base::Time::Now();
  navigation.http_status_code_ = 200;
  return navigation;
}

// static
SerializedNavigationEntry
SerializedNavigationEntryTestHelper::CreateNavigationForTest() {
  SerializedNavigationEntry navigation;
  navigation.index_ = test_data::kIndex;
  navigation.unique_id_ = test_data::kUniqueID;
  navigation.referrer_url_ = test_data::kReferrerURL;
  navigation.referrer_policy_ = test_data::kReferrerPolicy;
  navigation.virtual_url_ = test_data::kVirtualURL;
  navigation.title_ = test_data::kTitle;
  navigation.encoded_page_state_ = test_data::kEncodedPageState;
  navigation.transition_type_ = test_data::kTransitionType;
  navigation.has_post_data_ = test_data::kHasPostData;
  navigation.post_id_ = test_data::kPostID;
  navigation.original_request_url_ = test_data::kOriginalRequestURL;
  navigation.is_overriding_user_agent_ = test_data::kIsOverridingUserAgent;
  navigation.timestamp_ = test_data::kTimestamp;
  navigation.search_terms_ = test_data::kSearchTerms;
  navigation.favicon_url_ = test_data::kFaviconURL;
  navigation.http_status_code_ = test_data::kHttpStatusCode;

  navigation.redirect_chain_.push_back(test_data::kRedirectURL0);
  navigation.redirect_chain_.push_back(test_data::kRedirectURL1);
  navigation.redirect_chain_.push_back(test_data::kVirtualURL);
  return navigation;
}

// static
void SerializedNavigationEntryTestHelper::SetReferrerPolicy(
    int policy,
    SerializedNavigationEntry* navigation) {
  navigation->referrer_policy_ = policy;
}

// static
void SerializedNavigationEntryTestHelper::SetVirtualURL(
    const GURL& virtual_url,
    SerializedNavigationEntry* navigation) {
  navigation->virtual_url_ = virtual_url;
}

// static
void SerializedNavigationEntryTestHelper::SetEncodedPageState(
    const std::string& encoded_page_state,
    SerializedNavigationEntry* navigation) {
  navigation->encoded_page_state_ = encoded_page_state;
}

// static
void SerializedNavigationEntryTestHelper::SetTransitionType(
    ui::PageTransition transition_type,
    SerializedNavigationEntry* navigation) {
  navigation->transition_type_ = transition_type;
}

// static
void SerializedNavigationEntryTestHelper::SetHasPostData(
    bool has_post_data,
    SerializedNavigationEntry* navigation) {
  navigation->has_post_data_ = has_post_data;
}

// static
void SerializedNavigationEntryTestHelper::SetOriginalRequestURL(
    const GURL& original_request_url,
    SerializedNavigationEntry* navigation) {
  navigation->original_request_url_ = original_request_url;
}

// static
void SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(
    bool is_overriding_user_agent,
    SerializedNavigationEntry* navigation) {
  navigation->is_overriding_user_agent_ = is_overriding_user_agent;
}

// static
void SerializedNavigationEntryTestHelper::SetTimestamp(
    base::Time timestamp,
    SerializedNavigationEntry* navigation) {
  navigation->timestamp_ = timestamp;
}

}  // namespace sessions
