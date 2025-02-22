// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_channel_host_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/message_filter.h"

#if defined(MOJO_RUNNER_CLIENT)
#include "services/shell/runner/common/client_util.h"
#endif

namespace content {

BrowserGpuChannelHostFactory* BrowserGpuChannelHostFactory::instance_ = NULL;

class BrowserGpuChannelHostFactory::EstablishRequest
    : public base::RefCountedThreadSafe<EstablishRequest> {
 public:
  static scoped_refptr<EstablishRequest> Create(int gpu_client_id,
                                                uint64_t gpu_client_tracing_id,
                                                int gpu_host_id);
  void Wait();
  void Cancel();

  int gpu_host_id() { return gpu_host_id_; }
  IPC::ChannelHandle& channel_handle() { return channel_handle_; }
  gpu::GPUInfo gpu_info() { return gpu_info_; }

 private:
  friend class base::RefCountedThreadSafe<EstablishRequest>;
  explicit EstablishRequest(int gpu_client_id,
                            uint64_t gpu_client_tracing_id,
                            int gpu_host_id);
  ~EstablishRequest() {}
  void EstablishOnIO();
  void OnEstablishedOnIO(const IPC::ChannelHandle& channel_handle,
                         const gpu::GPUInfo& gpu_info);
  void FinishOnIO();
  void FinishOnMain();

  base::WaitableEvent event_;
  const int gpu_client_id_;
  const uint64_t gpu_client_tracing_id_;
  int gpu_host_id_;
  bool reused_gpu_process_;
  IPC::ChannelHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  bool finished_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

scoped_refptr<BrowserGpuChannelHostFactory::EstablishRequest>
BrowserGpuChannelHostFactory::EstablishRequest::Create(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id,
    int gpu_host_id) {
  scoped_refptr<EstablishRequest> establish_request =
      new EstablishRequest(gpu_client_id, gpu_client_tracing_id, gpu_host_id);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  // PostTask outside the constructor to ensure at least one reference exists.
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&BrowserGpuChannelHostFactory::EstablishRequest::EstablishOnIO,
                 establish_request));
  return establish_request;
}

BrowserGpuChannelHostFactory::EstablishRequest::EstablishRequest(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id,
    int gpu_host_id)
    : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      gpu_client_id_(gpu_client_id),
      gpu_client_tracing_id_(gpu_client_tracing_id),
      gpu_host_id_(gpu_host_id),
      reused_gpu_process_(false),
      finished_(false),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void BrowserGpuChannelHostFactory::EstablishRequest::EstablishOnIO() {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 "
          "BrowserGpuChannelHostFactory::EstablishRequest::EstablishOnIO"));
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED);
    if (!host) {
      LOG(ERROR) << "Failed to launch GPU process.";
      FinishOnIO();
      return;
    }
    gpu_host_id_ = host->host_id();
    reused_gpu_process_ = false;
  } else {
    if (reused_gpu_process_) {
      // We come here if we retried to establish the channel because of a
      // failure in ChannelEstablishedOnIO, but we ended up with the same
      // process ID, meaning the failure was not because of a channel error,
      // but another reason. So fail now.
      LOG(ERROR) << "Failed to create channel.";
      FinishOnIO();
      return;
    }
    reused_gpu_process_ = true;
  }

  bool preempts = true;
  bool allow_view_command_buffers = true;
  bool allow_real_time_streams = true;
  host->EstablishGpuChannel(
      gpu_client_id_, gpu_client_tracing_id_, preempts,
      allow_view_command_buffers, allow_real_time_streams,
      base::Bind(
          &BrowserGpuChannelHostFactory::EstablishRequest::OnEstablishedOnIO,
          this));
}

void BrowserGpuChannelHostFactory::EstablishRequest::OnEstablishedOnIO(
    const IPC::ChannelHandle& channel_handle,
    const gpu::GPUInfo& gpu_info) {
  if (!channel_handle.mojo_handle.is_valid() && reused_gpu_process_) {
    // We failed after re-using the GPU process, but it may have died in the
    // mean time. Retry to have a chance to create a fresh GPU process.
    DVLOG(1) << "Failed to create channel on existing GPU process. Trying to "
                "restart GPU process.";
    EstablishOnIO();
  } else {
    channel_handle_ = channel_handle;
    gpu_info_ = gpu_info;
    FinishOnIO();
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnIO() {
  event_.Signal();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserGpuChannelHostFactory::EstablishRequest::FinishOnMain,
                 this));
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnMain() {
  if (!finished_) {
    BrowserGpuChannelHostFactory* factory =
        BrowserGpuChannelHostFactory::instance();
    factory->GpuChannelEstablished();
    finished_ = true;
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::Wait() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "125248 BrowserGpuChannelHostFactory::EstablishRequest::Wait"));

    // We're blocking the UI thread, which is generally undesirable.
    // In this case we need to wait for this before we can show any UI
    // /anyway/, so it won't cause additional jank.
    // TODO(piman): Make this asynchronous (http://crbug.com/125248).
    TRACE_EVENT0("browser",
                 "BrowserGpuChannelHostFactory::EstablishGpuChannelSync");
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    event_.Wait();
  }
  FinishOnMain();
}

void BrowserGpuChannelHostFactory::EstablishRequest::Cancel() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  finished_ = true;
}

bool BrowserGpuChannelHostFactory::CanUseForTesting() {
  return GpuDataManager::GetInstance()->GpuAccessAllowed(NULL);
}

void BrowserGpuChannelHostFactory::Initialize(bool establish_gpu_channel) {
  DCHECK(!instance_);
  instance_ = new BrowserGpuChannelHostFactory();
  if (establish_gpu_channel) {
    instance_->EstablishGpuChannel(base::Closure());
  }
}

void BrowserGpuChannelHostFactory::Terminate() {
  DCHECK(instance_);
  delete instance_;
  instance_ = NULL;
}

BrowserGpuChannelHostFactory::BrowserGpuChannelHostFactory()
    : gpu_client_id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      gpu_client_tracing_id_(ChildProcessHost::kBrowserTracingProcessId),
      shutdown_event_(new base::WaitableEvent(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED)),
      gpu_memory_buffer_manager_(
          new BrowserGpuMemoryBufferManager(gpu_client_id_,
                                            gpu_client_tracing_id_)),
      gpu_host_id_(0) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    DCHECK(GetContentClient());
    base::FilePath cache_dir =
        GetContentClient()->browser()->GetShaderDiskCacheDirectory();
    if (!cache_dir.empty()) {
      GetIOThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(
              &BrowserGpuChannelHostFactory::InitializeShaderDiskCacheOnIO,
              gpu_client_id_, cache_dir));
    }
  }
}

BrowserGpuChannelHostFactory::~BrowserGpuChannelHostFactory() {
  DCHECK(IsMainThread());
  if (pending_request_.get())
    pending_request_->Cancel();
  for (size_t n = 0; n < established_callbacks_.size(); n++)
    established_callbacks_[n].Run();
  shutdown_event_->Signal();
  if (gpu_channel_) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = NULL;
  }
}

bool BrowserGpuChannelHostFactory::IsMainThread() {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserGpuChannelHostFactory::GetIOThreadTaskRunner() {
  return BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
}

std::unique_ptr<base::SharedMemory>
BrowserGpuChannelHostFactory::AllocateSharedMemory(size_t size) {
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return std::unique_ptr<base::SharedMemory>();
  return shm;
}

// Blocking the UI thread to open a GPU channel is not supported on Android.
// (Opening the initial channel to a child process involves handling a reply
// task on the UI thread first, so we cannot block here.)
#if !defined(OS_ANDROID)
scoped_refptr<gpu::GpuChannelHost>
BrowserGpuChannelHostFactory::EstablishGpuChannelSync() {
  EstablishGpuChannel(base::Closure());

  if (pending_request_.get())
    pending_request_->Wait();

  return gpu_channel_;
}
#endif

void BrowserGpuChannelHostFactory::EstablishGpuChannel(
    const base::Closure& callback) {
#if defined(MOJO_RUNNER_CLIENT)
  DCHECK(!shell::ShellIsRemote());
#endif
  if (gpu_channel_.get() && gpu_channel_->IsLost()) {
    DCHECK(!pending_request_.get());
    // Recreate the channel if it has been lost.
    gpu_channel_->DestroyChannel();
    gpu_channel_ = NULL;
  }

  if (!gpu_channel_.get() && !pending_request_.get()) {
    // We should only get here if the context was lost.
    pending_request_ = EstablishRequest::Create(
        gpu_client_id_, gpu_client_tracing_id_, gpu_host_id_);
  }

  if (!callback.is_null()) {
    if (gpu_channel_.get())
      callback.Run();
    else
      established_callbacks_.push_back(callback);
  }
}

gpu::GpuChannelHost* BrowserGpuChannelHostFactory::GetGpuChannel() {
  if (gpu_channel_.get() && !gpu_channel_->IsLost())
    return gpu_channel_.get();

  return NULL;
}

void BrowserGpuChannelHostFactory::GpuChannelEstablished() {
  DCHECK(IsMainThread());
  DCHECK(pending_request_.get());
  if (!pending_request_->channel_handle().mojo_handle.is_valid()) {
    DCHECK(!gpu_channel_.get());
  } else {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/466866
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile1(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "466866 BrowserGpuChannelHostFactory::GpuChannelEstablished1"));
    GetContentClient()->SetGpuInfo(pending_request_->gpu_info());
    gpu_channel_ = gpu::GpuChannelHost::Create(
        this, gpu_client_id_, pending_request_->gpu_info(),
        pending_request_->channel_handle(), shutdown_event_.get(),
        gpu_memory_buffer_manager_.get());
  }
  gpu_host_id_ = pending_request_->gpu_host_id();
  pending_request_ = NULL;

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/466866 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466866 BrowserGpuChannelHostFactory::GpuChannelEstablished2"));

  for (size_t n = 0; n < established_callbacks_.size(); n++)
    established_callbacks_[n].Run();

  established_callbacks_.clear();
}

// static
void BrowserGpuChannelHostFactory::AddFilterOnIO(
    int host_id,
    scoped_refptr<IPC::MessageFilter> filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->AddFilter(filter.get());
}

// static
void BrowserGpuChannelHostFactory::InitializeShaderDiskCacheOnIO(
    int gpu_client_id,
    const base::FilePath& cache_dir) {
  ShaderCacheFactory::GetInstance()->SetCacheInfo(gpu_client_id, cache_dir);
}

}  // namespace content
