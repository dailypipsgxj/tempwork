// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "content/browser/android/browser_jni_registrar.h"
#endif

namespace content {

namespace {

// Number of times to repost a Quit task so that the MessageLoop finishes up
// pending tasks and tasks posted by those pending tasks without risking the
// potential hang behavior of MessageLoop::QuitWhenIdle.
// The criteria for choosing this number: it should be high enough to make the
// quit act like QuitWhenIdle, while taking into account that any page which is
// animating may be rendering another frame for each quit deferral. For an
// animating page, the potential delay to quitting the RunLoop would be
// kNumQuitDeferrals * frame_render_time. Some perf tests run slow, such as
// 200ms/frame.
static const int kNumQuitDeferrals = 10;

static void DeferredQuitRunLoop(const base::Closure& quit_task,
                                int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    quit_task.Run();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DeferredQuitRunLoop, quit_task, num_quit_deferrals - 1));
  }
}

void RunAllPendingMessageAndSendQuit(BrowserThread::ID thread_id,
                                     const base::Closure& quit_task) {
  RunAllPendingInMessageLoop();
  BrowserThread::PostTask(thread_id, FROM_HERE, quit_task);
}

// Class used to handle result callbacks for ExecuteScriptAndGetValue.
class ScriptCallback {
 public:
  ScriptCallback() { }
  virtual ~ScriptCallback() { }
  void ResultCallback(const base::Value* result);

  std::unique_ptr<base::Value> result() { return std::move(result_); }

 private:
  std::unique_ptr<base::Value> result_;

  DISALLOW_COPY_AND_ASSIGN(ScriptCallback);
};

void ScriptCallback::ResultCallback(const base::Value* result) {
  if (result)
    result_.reset(result->DeepCopy());
  base::MessageLoop::current()->QuitWhenIdle();
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override {}

  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    processed_ = true;
  }

  // Returns true if any task was processed.
  bool processed() const { return processed_; }

 private:
  bool processed_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

// Adapter that makes a WindowedNotificationObserver::ConditionTestCallback from
// a WindowedNotificationObserver::ConditionTestCallbackWithoutSourceAndDetails
// by ignoring the notification source and details.
bool IgnoreSourceAndDetails(
    const WindowedNotificationObserver::
        ConditionTestCallbackWithoutSourceAndDetails& callback,
    const NotificationSource& source,
    const NotificationDetails& details) {
  return callback.Run();
}

}  // namespace

void RunMessageLoop() {
  base::RunLoop run_loop;
  RunThisRunLoop(&run_loop);
}

void RunThisRunLoop(base::RunLoop* run_loop) {
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());

  // If we're running inside a browser test, we might need to allow the test
  // launcher to do extra work before/after running a nested message loop.
  TestLauncherDelegate* delegate = NULL;
  delegate = GetCurrentTestLauncherDelegate();
  if (delegate)
    delegate->PreRunMessageLoop(run_loop);
  run_loop->Run();
  if (delegate)
    delegate->PostRunMessageLoop();
}

void RunAllPendingInMessageLoop() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, GetQuitTaskForRunLoop(&run_loop));
  RunThisRunLoop(&run_loop);
}

void RunAllPendingInMessageLoop(BrowserThread::ID thread_id) {
  if (BrowserThread::CurrentlyOn(thread_id)) {
    RunAllPendingInMessageLoop();
    return;
  }
  BrowserThread::ID current_thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&current_thread_id)) {
    NOTREACHED();
    return;
  }

  base::RunLoop run_loop;
  BrowserThread::PostTask(thread_id, FROM_HERE,
      base::Bind(&RunAllPendingMessageAndSendQuit, current_thread_id,
                 run_loop.QuitClosure()));
  RunThisRunLoop(&run_loop);
}

void RunAllBlockingPoolTasksUntilIdle() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    base::MessageLoop::current()->AddTaskObserver(&task_observer);
    base::RunLoop().RunUntilIdle();
    base::MessageLoop::current()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

base::Closure GetQuitTaskForRunLoop(base::RunLoop* run_loop) {
  return base::Bind(&DeferredQuitRunLoop, run_loop->QuitClosure(),
                    kNumQuitDeferrals);
}

std::unique_ptr<base::Value> ExecuteScriptAndGetValue(
    RenderFrameHost* render_frame_host,
    const std::string& script) {
  ScriptCallback observer;

  render_frame_host->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script),
      base::Bind(&ScriptCallback::ResultCallback, base::Unretained(&observer)));
  base::RunLoop().Run();
  return observer.result();
}

bool AreAllSitesIsolatedForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

void IsolateAllSitesForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kSitePerProcess);
}

#if defined(OS_ANDROID)
// Registers content/browser JNI bindings necessary for some types of tests.
bool RegisterJniForTesting(JNIEnv* env) {
  return content::android::RegisterBrowserJni(env);
}
#endif

MessageLoopRunner::MessageLoopRunner()
    : loop_running_(false),
      quit_closure_called_(false) {
}

MessageLoopRunner::~MessageLoopRunner() = default;

void MessageLoopRunner::Run() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not run the message loop if our quit closure has already been called.
  // This helps in scenarios where the closure has a chance to run before
  // we Run explicitly.
  if (quit_closure_called_)
    return;

  loop_running_ = true;
  RunThisRunLoop(&run_loop_);
}

base::Closure MessageLoopRunner::QuitClosure() {
  return base::Bind(&MessageLoopRunner::Quit, this);
}

void MessageLoopRunner::Quit() {
  DCHECK(thread_checker_.CalledOnValidThread());

  quit_closure_called_ = true;

  // Only run the quit task if we are running the message loop.
  if (loop_running_) {
    GetQuitTaskForRunLoop(&run_loop_).Run();
    loop_running_ = false;
  }
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  AddNotificationType(notification_type, source);
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const ConditionTestCallback& callback)
    : seen_(false),
      running_(false),
      callback_(callback),
      source_(NotificationService::AllSources()) {
  AddNotificationType(notification_type, source_);
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const ConditionTestCallbackWithoutSourceAndDetails& callback)
    : seen_(false),
      running_(false),
      callback_(base::Bind(&IgnoreSourceAndDetails, callback)),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source_);
}

WindowedNotificationObserver::~WindowedNotificationObserver() {}

void WindowedNotificationObserver::AddNotificationType(
    int notification_type,
    const NotificationSource& source) {
  registrar_.Add(this, notification_type, source);
}

void WindowedNotificationObserver::Wait() {
  if (seen_)
    return;

  running_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void WindowedNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  if (!callback_.is_null() && !callback_.Run(source, details))
    return;

  seen_ = true;
  if (!running_)
    return;

  message_loop_runner_->Quit();
  running_ = false;
}

InProcessUtilityThreadHelper::InProcessUtilityThreadHelper()
    : child_thread_count_(0) {
  RenderProcessHost::SetRunRendererInProcess(true);
  BrowserChildProcessObserver::Add(this);
}

InProcessUtilityThreadHelper::~InProcessUtilityThreadHelper() {
  if (child_thread_count_) {
    DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::UI));
    DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::IO));
    runner_ = new MessageLoopRunner;
    runner_->Run();
  }
  BrowserChildProcessObserver::Remove(this);
  RenderProcessHost::SetRunRendererInProcess(false);
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostConnected(
    const ChildProcessData& data) {
  child_thread_count_++;
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  if (--child_thread_count_)
    return;

  if (runner_.get())
    runner_->Quit();
}

RenderFrameDeletedObserver::RenderFrameDeletedObserver(RenderFrameHost* rfh)
    : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
      process_id_(rfh->GetProcess()->GetID()),
      routing_id_(rfh->GetRoutingID()),
      deleted_(false) {}

RenderFrameDeletedObserver::~RenderFrameDeletedObserver() {}

void RenderFrameDeletedObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetProcess()->GetID() == process_id_ &&
      render_frame_host->GetRoutingID() == routing_id_) {
    deleted_ = true;

    if (runner_.get())
      runner_->Quit();
  }
}

bool RenderFrameDeletedObserver::deleted() {
  return deleted_;
}

void RenderFrameDeletedObserver::WaitUntilDeleted() {
  if (deleted_)
    return;

  runner_.reset(new base::RunLoop());
  runner_->Run();
  runner_.reset();
}

WebContentsDestroyedWatcher::WebContentsDestroyedWatcher(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      message_loop_runner_(new MessageLoopRunner) {
  EXPECT_TRUE(web_contents != NULL);
}

WebContentsDestroyedWatcher::~WebContentsDestroyedWatcher() {}

void WebContentsDestroyedWatcher::Wait() {
  message_loop_runner_->Run();
}

void WebContentsDestroyedWatcher::WebContentsDestroyed() {
  message_loop_runner_->Quit();
}

}  // namespace content
