// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"

using content::DownloadItem;
using safe_browsing::DownloadFileType;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::_;

namespace {

// Create a char array that has as many elements as there are download
// interrupt reasons. We can then use that in a static_assert to make sure
// that all the interrupt reason codes are accounted for. The reason codes are
// unfortunately sparse, making this necessary.
char kInterruptReasonCounter[] = {
  0,                                // content::DOWNLOAD_INTERRUPT_REASON_NONE
#define INTERRUPT_REASON(name,value) 0,
#include "content/public/browser/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};
const size_t kInterruptReasonCount = arraysize(kInterruptReasonCounter);

// Default target path for a mock download item in DownloadItemModelTest.
const base::FilePath::CharType kDefaultTargetFilePath[] =
    FILE_PATH_LITERAL("/foo/bar/foo.bar");

const base::FilePath::CharType kDefaultDisplayFileName[] =
    FILE_PATH_LITERAL("foo.bar");

// Default URL for a mock download item in DownloadItemModelTest.
const char kDefaultURL[] = "http://example.com/foo.bar";

class DownloadItemModelTest : public testing::Test {
 public:
  DownloadItemModelTest()
      : model_(&item_) {}

  virtual ~DownloadItemModelTest() {
  }

 protected:
  // Sets up defaults for the download item and sets |model_| to a new
  // DownloadItemModel that uses the mock download item.
  void SetupDownloadItemDefaults() {
    ON_CALL(item_, GetReceivedBytes()).WillByDefault(Return(1));
    ON_CALL(item_, GetTotalBytes()).WillByDefault(Return(2));
    ON_CALL(item_, TimeRemaining(_)).WillByDefault(Return(false));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, AllDataSaved()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    ON_CALL(item_, GetState())
        .WillByDefault(Return(DownloadItem::IN_PROGRESS));
    ON_CALL(item_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kDefaultURL)));
    ON_CALL(item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(kDefaultDisplayFileName)));
    ON_CALL(item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath(kDefaultTargetFilePath)));
    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(
            Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    ON_CALL(item_, IsPaused()).WillByDefault(Return(false));
  }

  void SetupInterruptedDownloadItem(content::DownloadInterruptReason reason) {
    EXPECT_CALL(item_, GetLastReason()).WillRepeatedly(Return(reason));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(
            (reason == content::DOWNLOAD_INTERRUPT_REASON_NONE) ?
                DownloadItem::IN_PROGRESS :
                DownloadItem::INTERRUPTED));
  }

  content::MockDownloadItem& item() {
    return item_;
  }

  DownloadItemModel& model() {
    return model_;
  }

 private:
  NiceMock<content::MockDownloadItem> item_;
  DownloadItemModel model_;
};

}  // namespace

TEST_F(DownloadItemModelTest, InterruptedStatus) {
  // Test that we have the correct interrupt status message for downloads that
  // are in the INTERRUPTED state.
  const struct TestCase {
    // The reason.
    content::DownloadInterruptReason reason;

    // Expected status string. This will include the progress as well.
    const char* expected_status;
  } kTestCases[] = {
      {content::DOWNLOAD_INTERRUPT_REASON_NONE, "1/2 B"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       "Failed - Download error"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
       "Failed - Insufficient permissions"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, "Failed - Disk full"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       "Failed - Path too long"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
       "Failed - File too large"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
       "Failed - Virus detected"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, "Failed - Blocked"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
       "Failed - Virus scan failed"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
       "Failed - File truncated"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
       "Failed - System busy"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
       "Failed - Download error"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
       "Failed - Network error"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
       "Failed - Network timeout"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
       "Failed - Network disconnected"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
       "Failed - Server unavailable"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
       "Failed - Network error"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
       "Failed - Server problem"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
       "Failed - Download error"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
       "Failed - No file"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
       "Failed - Needs authorization"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
       "Failed - Bad certificate"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
       "Failed - Forbidden"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
       "Failed - Server unreachable"},
      {content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, "Canceled"},
      {content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, "Failed - Shutdown"},
      {content::DOWNLOAD_INTERRUPT_REASON_CRASH, "Failed - Crash"},
  };
  static_assert(kInterruptReasonCount == arraysize(kTestCases),
                "interrupt reason mismatch");

  SetupDownloadItemDefaults();
  for (unsigned i = 0; i < arraysize(kTestCases); ++i) {
    const TestCase& test_case = kTestCases[i];
    SetupInterruptedDownloadItem(test_case.reason);
    EXPECT_STREQ(test_case.expected_status,
                 base::UTF16ToUTF8(model().GetStatusText()).c_str());
  }
}

// Note: This test is currently skipped on Android. See http://crbug.com/139398
TEST_F(DownloadItemModelTest, InterruptTooltip) {
  // Test that we have the correct interrupt tooltip for downloads that are in
  // the INTERRUPTED state.
  const struct TestCase {
    // The reason.
    content::DownloadInterruptReason reason;

    // Expected tooltip text. The tooltip text for interrupted downloads
    // typically consist of two lines. One for the filename and one for the
    // interrupt reason. The returned string contains a newline.
    const char* expected_tooltip;
  } kTestCases[] = {
      {content::DOWNLOAD_INTERRUPT_REASON_NONE, "foo.bar"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       "foo.bar\nDownload error"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
       "foo.bar\nInsufficient permissions"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, "foo.bar\nDisk full"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       "foo.bar\nPath too long"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
       "foo.bar\nFile too large"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
       "foo.bar\nVirus detected"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, "foo.bar\nBlocked"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
       "foo.bar\nVirus scan failed"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
       "foo.bar\nFile truncated"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
       "foo.bar\nSystem busy"},
      {content::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
       "foo.bar\nDownload error"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
       "foo.bar\nNetwork error"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
       "foo.bar\nNetwork timeout"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
       "foo.bar\nNetwork disconnected"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
       "foo.bar\nServer unavailable"},
      {content::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
       "foo.bar\nNetwork error"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
       "foo.bar\nServer problem"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
       "foo.bar\nDownload error"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
       "foo.bar\nNo file"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
       "foo.bar\nNeeds authorization"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
       "foo.bar\nBad certificate"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
       "foo.bar\nForbidden"},
      {content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
       "foo.bar\nServer unreachable"},
      {content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, "foo.bar"},
      {content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, "foo.bar\nShutdown"},
      {content::DOWNLOAD_INTERRUPT_REASON_CRASH, "foo.bar\nCrash"},
  };
  static_assert(kInterruptReasonCount == arraysize(kTestCases),
                "interrupt reason mismatch");

  // Large tooltip width. Should be large enough to accommodate the entire
  // tooltip without truncation.
  const int kLargeTooltipWidth = 1000;

  // Small tooltip width. Small enough to require truncation of most
  // tooltips. Used to test eliding logic.
  const int kSmallTooltipWidth = 40;

  const gfx::FontList& font_list =
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont);
  SetupDownloadItemDefaults();
  for (unsigned i = 0; i < arraysize(kTestCases); ++i) {
    const TestCase& test_case = kTestCases[i];
    SetupInterruptedDownloadItem(test_case.reason);

    // GetTooltipText() elides the tooltip so that the text would fit within a
    // given width. The following test would fail if kLargeTooltipWidth isn't
    // large enough to accomodate all the strings.
    EXPECT_STREQ(
        test_case.expected_tooltip,
        base::UTF16ToUTF8(model().GetTooltipText(font_list,
                                                 kLargeTooltipWidth)).c_str());

    // Check that if the width is small, the returned tooltip only contains
    // lines of the given width or smaller.
    base::string16 truncated_tooltip =
        model().GetTooltipText(font_list, kSmallTooltipWidth);
    for (const base::string16& line :
         base::SplitString(truncated_tooltip, base::ASCIIToUTF16("\n"),
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY))
      EXPECT_GE(kSmallTooltipWidth, gfx::GetStringWidth(line, font_list));
  }
}

TEST_F(DownloadItemModelTest, InProgressStatus) {
  const struct TestCase {
    int64_t received_bytes;             // Return value of GetReceivedBytes().
    int64_t total_bytes;                // Return value of GetTotalBytes().
    bool  time_remaining_known;         // If TimeRemaining() is known.
    bool  open_when_complete;           // GetOpenWhenComplete().
    bool  is_paused;                    // IsPaused().
    const char* expected_status;        // Expected status text.
  } kTestCases[] = {
    // These are all the valid combinations of the above fields for a download
    // that is in IN_PROGRESS state. Go through all of them and check the return
    // value of DownloadItemModel::GetStatusText(). The point isn't to lock down
    // the status strings, but to make sure we end up with something sane for
    // all the circumstances we care about.
    //
    // For GetReceivedBytes()/GetTotalBytes(), we only check whether each is
    // non-zero. In addition, if |total_bytes| is zero, then
    // |time_remaining_known| is also false.
    //
    //         .-- .TimeRemaining() is known.
    //        |       .-- .GetOpenWhenComplete()
    //        |      |      .---- .IsPaused()
    { 0, 0, false, false, false, "Starting\xE2\x80\xA6" },
    { 1, 0, false, false, false, "1 B" },
    { 0, 2, false, false, false, "Starting\xE2\x80\xA6"},
    { 1, 2, false, false, false, "1/2 B" },
    { 0, 2, true,  false, false, "0/2 B, 10 secs left" },
    { 1, 2, true,  false, false, "1/2 B, 10 secs left" },
    { 0, 0, false, true,  false, "Opening when complete" },
    { 1, 0, false, true,  false, "Opening when complete" },
    { 0, 2, false, true,  false, "Opening when complete" },
    { 1, 2, false, true,  false, "Opening when complete" },
    { 0, 2, true,  true,  false, "Opening in 10 secs\xE2\x80\xA6"},
    { 1, 2, true,  true,  false, "Opening in 10 secs\xE2\x80\xA6"},
    { 0, 0, false, false, true,  "0 B, Paused" },
    { 1, 0, false, false, true,  "1 B, Paused" },
    { 0, 2, false, false, true,  "0/2 B, Paused" },
    { 1, 2, false, false, true,  "1/2 B, Paused" },
    { 0, 2, true,  false, true,  "0/2 B, Paused" },
    { 1, 2, true,  false, true,  "1/2 B, Paused" },
    { 0, 0, false, true,  true,  "0 B, Paused" },
    { 1, 0, false, true,  true,  "1 B, Paused" },
    { 0, 2, false, true,  true,  "0/2 B, Paused" },
    { 1, 2, false, true,  true,  "1/2 B, Paused" },
    { 0, 2, true,  true,  true,  "0/2 B, Paused" },
    { 1, 2, true,  true,  true,  "1/2 B, Paused" },
  };

  SetupDownloadItemDefaults();

  for (unsigned i = 0; i < arraysize(kTestCases); i++) {
    const TestCase& test_case = kTestCases[i];
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
    EXPECT_CALL(item(), GetReceivedBytes())
        .WillRepeatedly(Return(test_case.received_bytes));
    EXPECT_CALL(item(), GetTotalBytes())
        .WillRepeatedly(Return(test_case.total_bytes));
    EXPECT_CALL(item(), TimeRemaining(_))
        .WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<0>(base::TimeDelta::FromSeconds(10)),
            Return(test_case.time_remaining_known)));
    EXPECT_CALL(item(), GetOpenWhenComplete())
        .WillRepeatedly(Return(test_case.open_when_complete));
    EXPECT_CALL(item(), IsPaused())
        .WillRepeatedly(Return(test_case.is_paused));

    EXPECT_STREQ(test_case.expected_status,
                 base::UTF16ToUTF8(model().GetStatusText()).c_str());
  }
}

TEST_F(DownloadItemModelTest, ShouldShowInShelf) {
  SetupDownloadItemDefaults();

  // By default the download item should be displayable on the shelf.
  EXPECT_TRUE(model().ShouldShowInShelf());

  // Once explicitly set, ShouldShowInShelf() should return the explicit value.
  model().SetShouldShowInShelf(false);
  EXPECT_FALSE(model().ShouldShowInShelf());

  model().SetShouldShowInShelf(true);
  EXPECT_TRUE(model().ShouldShowInShelf());
}

TEST_F(DownloadItemModelTest, DangerLevel) {
  SetupDownloadItemDefaults();

  // Default danger level is NOT_DANGEROUS.
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS, model().GetDangerLevel());

  model().SetDangerLevel(DownloadFileType::ALLOW_ON_USER_GESTURE);
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE, model().GetDangerLevel());
}

TEST_F(DownloadItemModelTest, ShouldRemoveFromShelfWhenComplete) {
  const struct TestCase {
    DownloadItem::DownloadState state;
    bool is_dangerous;  // Expectation for IsDangerous().
    bool is_auto_open;  // Expectation for GetOpenWhenComplete().
    bool auto_opened;   // Whether the download was successfully
                        // auto-opened. Expecation for GetAutoOpened().
    bool expected_result;
  } kTestCases[] = {
    // All the valid combinations of state, is_dangerous, is_auto_open and
    // auto_opened.
    //
    //                              .--- Is dangerous.
    //                             |       .--- Auto open or temporary.
    //                             |      |      .--- Auto opened.
    //                             |      |      |      .--- Expected result.
    { DownloadItem::IN_PROGRESS, false, false, false, false},
    { DownloadItem::IN_PROGRESS, false, true , false, true },
    { DownloadItem::IN_PROGRESS, true , false, false, false},
    { DownloadItem::IN_PROGRESS, true , true , false, false},
    { DownloadItem::COMPLETE,    false, false, false, false},
    { DownloadItem::COMPLETE,    false, true , false, false},
    { DownloadItem::COMPLETE,    false, false, true , true },
    { DownloadItem::COMPLETE,    false, true , true , true },
    { DownloadItem::CANCELLED,   false, false, false, false},
    { DownloadItem::CANCELLED,   false, true , false, false},
    { DownloadItem::CANCELLED,   true , false, false, false},
    { DownloadItem::CANCELLED,   true , true , false, false},
    { DownloadItem::INTERRUPTED, false, false, false, false},
    { DownloadItem::INTERRUPTED, false, true , false, false},
    { DownloadItem::INTERRUPTED, true , false, false, false},
    { DownloadItem::INTERRUPTED, true , true , false, false}
  };

  SetupDownloadItemDefaults();

  for (unsigned i = 0; i < arraysize(kTestCases); i++) {
    const TestCase& test_case = kTestCases[i];
    EXPECT_CALL(item(), GetOpenWhenComplete())
        .WillRepeatedly(Return(test_case.is_auto_open));
    EXPECT_CALL(item(), GetState())
        .WillRepeatedly(Return(test_case.state));
    EXPECT_CALL(item(), IsDangerous())
        .WillRepeatedly(Return(test_case.is_dangerous));
    EXPECT_CALL(item(), GetAutoOpened())
        .WillRepeatedly(Return(test_case.auto_opened));

    EXPECT_EQ(test_case.expected_result,
              model().ShouldRemoveFromShelfWhenComplete())
        << "Test case: " << i;
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
  }
}
