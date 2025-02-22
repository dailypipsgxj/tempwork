// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

class RulesetDistributionListener : public RulesetDistributor {
 public:
  RulesetDistributionListener() {}
  ~RulesetDistributionListener() override {}

  void AwaitDistribution() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void PublishNewVersion(base::File) override {
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RulesetDistributionListener);
};

}  // namespace

using subresource_filter::testing::ScopedSubresourceFilterFeatureToggle;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

class SubresourceFilterBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceFilterBrowserTest() {}
  ~SubresourceFilterBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    scoped_feature_toggle_.reset(new ScopedSubresourceFilterFeatureToggle(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) {
    for (content::RenderFrameHost* frame : web_contents()->GetAllFrames()) {
      if (frame->GetFrameName() == name)
        return frame;
    }
    return nullptr;
  }

  bool WasScriptResourceLoaded(content::RenderFrameHost* rfh) {
    DCHECK(rfh);
    bool script_resource_was_loaded;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        rfh, "domAutomationController.send(!!document.scriptExecuted)",
        &script_resource_was_loaded));
    return script_resource_was_loaded;
  }

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix) {
    // For simplicity, use the |suffix| itself as the uniquely identifying
    // version tag.
    const std::string& test_ruleset_content_version(suffix);
    TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    RulesetDistributionListener* listener = new RulesetDistributionListener();
    g_browser_process->subresource_filter_ruleset_service()
        ->RegisterDistributor(base::WrapUnique(listener));
    subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
    unindexed_ruleset_info.content_version = test_ruleset_content_version;
    unindexed_ruleset_info.ruleset_path = test_ruleset_pair.unindexed.path;
    g_browser_process->subresource_filter_ruleset_service()
        ->IndexAndStoreAndPublishRulesetIfNeeded(unindexed_ruleset_info);
    listener->AwaitDistribution();
  }

 private:
  std::unique_ptr<ScopedSubresourceFilterFeatureToggle> scoped_feature_toggle_;
  TestRulesetCreator ruleset_creator_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
  const GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("subresource_filter")),
      base::FilePath(FILE_PATH_LITERAL("frame_with_included_script.html"))));

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasScriptResourceLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasScriptResourceLoaded(web_contents()->GetMainFrame()));

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasScriptResourceLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  const GURL url(ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("subresource_filter"),
      base::FilePath().AppendASCII("frame_set.html")));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  const char* kSubframeNames[] = {"one", "two"};
  for (const char* subframe_name : kSubframeNames) {
    content::RenderFrameHost* frame = FindFrameByName(subframe_name);
    ASSERT_TRUE(frame);
    EXPECT_FALSE(WasScriptResourceLoaded(frame));
  }
}

}  // namespace subresource_filter
