// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

InstallableParams GetManifestParams() {
  InstallableParams params;
  params.check_installable = false;
  params.fetch_valid_icon = false;
  return params;
}

InstallableParams GetWebAppParams() {
  InstallableParams params = GetManifestParams();
  params.ideal_icon_size_in_dp = 48;
  params.minimum_icon_size_in_dp = 48;
  params.check_installable = true;
  params.fetch_valid_icon = true;
  return params;
}

InstallableParams GetIconParams() {
  InstallableParams params = GetManifestParams();
  params.ideal_icon_size_in_dp = 48;
  params.minimum_icon_size_in_dp = 48;
  params.fetch_valid_icon = true;
  return params;
}

}  // anonymous namespace

class CallbackTester {
 public:
  explicit CallbackTester(base::Closure quit_closure)
      : quit_closure_(quit_closure) { }

  void OnDidFinishInstallableCheck(const InstallableData& data) {
    error_code_ = data.error_code;
    manifest_url_ = data.manifest_url;
    manifest_ = data.manifest;
    icon_url_ = data.icon_url;
    if (data.icon)
      icon_.reset(new SkBitmap(*data.icon));
    is_installable_ = data.is_installable;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  InstallableErrorCode error_code() const { return error_code_; }
  const GURL& manifest_url() const { return manifest_url_; }
  const content::Manifest& manifest() const { return manifest_; }
  const GURL& icon_url() const { return icon_url_; }
  const SkBitmap* icon() const { return icon_.get(); }
  bool is_installable() const { return is_installable_; }

 private:
  base::Closure quit_closure_;
  InstallableErrorCode error_code_;
  GURL manifest_url_;
  content::Manifest manifest_;
  GURL icon_url_;
  std::unique_ptr<SkBitmap> icon_;
  bool is_installable_;
};

class NestedCallbackTester {
 public:
  NestedCallbackTester(InstallableManager* manager,
                       const InstallableParams& params,
                       base::Closure quit_closure)
      : manager_(manager), params_(params), quit_closure_(quit_closure) {}

  void Run() {
    manager_->GetData(params_,
                      base::Bind(&NestedCallbackTester::OnDidFinishFirstCheck,
                                 base::Unretained(this)));
  }

  void OnDidFinishFirstCheck(const InstallableData& data) {
    error_code_ = data.error_code;
    manifest_url_ = data.manifest_url;
    manifest_ = data.manifest;
    icon_url_ = data.icon_url;
    if (data.icon)
      icon_.reset(new SkBitmap(*data.icon));
    is_installable_ = data.is_installable;

    manager_->GetData(params_,
                      base::Bind(&NestedCallbackTester::OnDidFinishSecondCheck,
                                 base::Unretained(this)));
  }

  void OnDidFinishSecondCheck(const InstallableData& data) {
    EXPECT_EQ(error_code_, data.error_code);
    EXPECT_EQ(manifest_url_, data.manifest_url);
    EXPECT_EQ(icon_url_, data.icon_url);
    EXPECT_EQ(icon_.get(), data.icon);
    EXPECT_EQ(is_installable_, data.is_installable);
    EXPECT_EQ(manifest_.IsEmpty(), data.manifest.IsEmpty());
    EXPECT_EQ(manifest_.start_url, data.manifest.start_url);
    EXPECT_EQ(manifest_.display, data.manifest.display);
    EXPECT_EQ(manifest_.name, data.manifest.name);
    EXPECT_EQ(manifest_.short_name, data.manifest.short_name);

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

 private:
  InstallableManager* manager_;
  InstallableParams params_;
  base::Closure quit_closure_;
  InstallableErrorCode error_code_;
  GURL manifest_url_;
  content::Manifest manifest_;
  GURL icon_url_;
  std::unique_ptr<SkBitmap> icon_;
  bool is_installable_;
};

class InstallableManagerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateAndRunInstallableManager(CallbackTester* tester,
                                        const InstallableParams& params,
                                        const std::string& url) {
    GURL test_url = embedded_test_server()->GetURL(url);
    ui_test_utils::NavigateToURL(browser(), test_url);
    RunInstallableManager(tester, params);
  }

  void RunInstallableManager(CallbackTester* tester,
                             const InstallableParams& params) {
    InstallableManager* manager = GetManager();
    manager->GetData(params,
                     base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                                base::Unretained(tester)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure app banners are disabled in the browser so they do not
    // interfere with the test.
    command_line->AppendSwitch(switches::kDisableAddToShelf);
  }

  InstallableManager* GetManager() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InstallableManager::CreateForWebContents(web_contents);
    InstallableManager* manager =
        InstallableManager::FromWebContents(web_contents);
    CHECK(manager);

    return manager;
  }

};

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManagerBeginsInEmptyState) {
  // Ensure that the InstallableManager starts off with everything null.
  InstallableManager* manager = GetManager();

  EXPECT_TRUE(manager->manifest().IsEmpty());
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_TRUE(manager->icons_.empty());
  EXPECT_FALSE(manager->is_installable());

  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->installable_error());
  EXPECT_TRUE(manager->tasks_.empty());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckNoManifest) {
  // Ensure that a page with no manifest returns the appropriate error and with
  // null fields for everything.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/no_manifest_test_page.html");
  run_loop.Run();

  // If there is no manifest, everything should be empty.
  EXPECT_TRUE(tester->manifest().IsEmpty());
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(NO_MANIFEST, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifest404) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/manifest_bad_link.html");
  run_loop.Run();

  // The installable manager should return a manifest URL even if it 404s.
  // However, the check should fail with a ManifestEmpty error.
  EXPECT_TRUE(tester->manifest().IsEmpty());

  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(MANIFEST_EMPTY, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestOnly) {
  // Verify that asking for just the manifest works as expected.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckInstallableParamsDefaultConstructor) {
  // Verify that using InstallableParams' default constructor is equivalent to
  // just asking for the manifest alone.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params;
  NavigateAndRunInstallableManager(tester.get(), params,
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithOnlyRelatedApplications) {
  // This page has a manifest with only related applications specified. Asking
  // for just the manifest should succeed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                     "/banners/play_app_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }

  // Ask for an icon (but don't navigate). This should fail with
  // NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetIconParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  // Ask for everything. This should fail with NO_ACCEPTABLE_ICON - the icon
  // fetch has already failed, so that cached error stops the installable check
  // from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  // Ask for a different size icon. This should fail with START_URL_NOT_VALID
  // since we won't have a cached icon error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.ideal_icon_size_in_dp = 32;
    params.minimum_icon_size_in_dp = 32;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(START_URL_NOT_VALID, tester->error_code());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestAndIcon) {
  // Add to homescreen checks for manifest + icon.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetIconParams(),
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->icon_url().is_empty());
  EXPECT_NE(nullptr, tester->icon());

  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebapp) {
  // Request everything.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_FALSE(tester->icon_url().is_empty());
    EXPECT_NE(nullptr, tester->icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(manager->manifest().IsEmpty());
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->is_installable());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE((manager->icon_url({48,48}).is_empty()));
    EXPECT_NE(nullptr, (manager->icon({48,48})));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->installable_error());
    EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error({48,48})));
    EXPECT_TRUE(manager->tasks_.empty());
  }

  // Request everything again without navigating away. This should work fine.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_FALSE(tester->icon_url().is_empty());
    EXPECT_NE(nullptr, tester->icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(manager->manifest().IsEmpty());
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->is_installable());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE((manager->icon_url({48,48}).is_empty()));
    EXPECT_NE(nullptr, (manager->icon({48,48})));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->installable_error());
    EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error({48,48})));
    EXPECT_TRUE(manager->tasks_.empty());
  }

  {
    // Check that a subsequent navigation resets state.
    ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
    InstallableManager* manager = GetManager();

    EXPECT_TRUE(manager->manifest().IsEmpty());
    EXPECT_TRUE(manager->manifest_url().is_empty());
    EXPECT_FALSE(manager->is_installable());
    EXPECT_TRUE(manager->icons_.empty());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->installable_error());
    EXPECT_TRUE(manager->tasks_.empty());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebappInIframe) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                   "/banners/iframe_test_page.html");
  run_loop.Run();

  // The installable manager should only retrieve items in the main frame;
  // everything should be empty here.
  EXPECT_TRUE(tester->manifest().IsEmpty());
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_EQ(NO_MANIFEST, tester->error_code());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckPageWithManifestAndNoServiceWorker) {
  // Just fetch the manifest. This should have no error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetManifestParams(),
        "/banners/manifest_no_service_worker.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }

  // Fetch the full criteria should fail.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_MATCHING_SERVICE_WORKER, tester->error_code());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckChangeInIconDimensions) {
  // Verify that a follow-up request for an icon with a different size works.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_FALSE(tester->icon_url().is_empty());
    EXPECT_NE(nullptr, tester->icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Dial up the icon size requirements to something that isn't available.
    // This should now fail with NoIconMatchingRequirements.
    InstallableParams params = GetWebAppParams();
    params.ideal_icon_size_in_dp = 2000;
    params.minimum_icon_size_in_dp = 2000;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  // Navigate and verify the reverse: an overly large icon requested first
  // fails, but a smaller icon requested second passes.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // This should fail with NoIconMatchingRequirements.
    InstallableParams params = GetWebAppParams();
    params.ideal_icon_size_in_dp = 2000;
    params.minimum_icon_size_in_dp = 2000;
    NavigateAndRunInstallableManager(tester.get(), params,
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    RunInstallableManager(tester.get(), GetWebAppParams());

    run_loop.Run();

    // The smaller icon requirements should allow this to pass.
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_FALSE(tester->icon_url().is_empty());
    EXPECT_NE(nullptr, tester->icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNestedCallsToGetData) {
  // Verify that we can call GetData while in a callback from GetData.
  base::RunLoop run_loop;
  InstallableParams params = GetWebAppParams();
  std::unique_ptr<NestedCallbackTester> tester(
      new NestedCallbackTester(GetManager(), params, run_loop.QuitClosure()));

  tester->Run();
  run_loop.Run();
}
