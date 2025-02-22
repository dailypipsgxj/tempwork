// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include <windows.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

void SetOffset(OVERLAPPED* overlapped, const LARGE_INTEGER& offset) {
  overlapped->Offset = offset.LowPart;
  overlapped->OffsetHigh = offset.HighPart;
}

void IncrementOffset(OVERLAPPED* overlapped, DWORD count) {
  LARGE_INTEGER offset;
  offset.LowPart = overlapped->Offset;
  offset.HighPart = overlapped->OffsetHigh;
  offset.QuadPart += static_cast<LONGLONG>(count);
  SetOffset(overlapped, offset);
}

}  // namespace

FileStream::Context::Context(const scoped_refptr<base::TaskRunner>& task_runner)
    : async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner),
      async_read_initiated_(false),
      async_read_completed_(false),
      io_complete_for_read_received_(false),
      result_(0) {}

FileStream::Context::Context(base::File file,
                             const scoped_refptr<base::TaskRunner>& task_runner)
    : file_(std::move(file)),
      async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner),
      async_read_initiated_(false),
      async_read_completed_(false),
      io_complete_for_read_received_(false),
      result_(0) {
  if (file_.IsValid()) {
    DCHECK(file_.async());
    OnFileOpened();
  }
}

FileStream::Context::~Context() {
}

int FileStream::Context::Read(IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  CHECK(!async_in_progress_);
  DCHECK(!async_read_initiated_);
  DCHECK(!async_read_completed_);
  DCHECK(!io_complete_for_read_received_);

  IOCompletionIsPending(callback, buf);

  async_read_initiated_ = true;
  result_ = 0;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FileStream::Context::ReadAsync, base::Unretained(this),
                 file_.GetPlatformFile(), make_scoped_refptr(buf), buf_len,
                 &io_context_.overlapped, base::ThreadTaskRunnerHandle::Get()));
  return ERR_IO_PENDING;
}

int FileStream::Context::Write(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) {
  CHECK(!async_in_progress_);

  result_ = 0;

  DWORD bytes_written = 0;
  if (!WriteFile(file_.GetPlatformFile(), buf->data(), buf_len,
                 &bytes_written, &io_context_.overlapped)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_IO_PENDING)
      IOCompletionIsPending(callback, buf);
    else
      LOG(WARNING) << "WriteFile failed: " << error.os_error;
    return static_cast<int>(error.result);
  }

  IOCompletionIsPending(callback, buf);
  return ERR_IO_PENDING;
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(
    int64_t offset) {
  LARGE_INTEGER result;
  result.QuadPart = offset;
  SetOffset(&io_context_.overlapped, result);
  return IOResult(result.QuadPart, 0);
}

void FileStream::Context::OnFileOpened() {
  base::MessageLoopForIO::current()->RegisterIOHandler(file_.GetPlatformFile(),
                                                       this);
}

void FileStream::Context::IOCompletionIsPending(
    const CompletionCallback& callback,
    IOBuffer* buf) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  in_flight_buf_ = buf;  // Hold until the async operation ends.
  async_in_progress_ = true;
}

void FileStream::Context::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_read,
    DWORD error) {
  DCHECK_EQ(&io_context_, context);
  DCHECK(!callback_.is_null());
  DCHECK(async_in_progress_);

  if (!async_read_initiated_)
    async_in_progress_ = false;

  if (orphaned_) {
    io_complete_for_read_received_ = true;
    // If we are called due to a pending read and the asynchronous read task
    // has not completed we have to keep the context around until it completes.
    if (async_read_initiated_ && !async_read_completed_)
      return;
    DeleteOrphanedContext();
    return;
  }

  if (error == ERROR_HANDLE_EOF) {
    result_ = 0;
  } else if (error) {
    IOResult error_result = IOResult::FromOSError(error);
    result_ = static_cast<int>(error_result.result);
  } else {
    if (result_)
      DCHECK_EQ(result_, static_cast<int>(bytes_read));
    result_ = bytes_read;
    IncrementOffset(&io_context_.overlapped, bytes_read);
  }

  if (async_read_initiated_)
    io_complete_for_read_received_ = true;

  InvokeUserCallback();
}

void FileStream::Context::InvokeUserCallback() {
  // For an asynchonous Read operation don't invoke the user callback until
  // we receive the IO completion notification and the asynchronous Read
  // completion notification.
  if (async_read_initiated_) {
    if (!io_complete_for_read_received_ || !async_read_completed_)
      return;
    async_read_initiated_ = false;
    io_complete_for_read_received_ = false;
    async_read_completed_ = false;
    async_in_progress_ = false;
  }
  CompletionCallback temp_callback = callback_;
  callback_.Reset();
  scoped_refptr<IOBuffer> temp_buf = in_flight_buf_;
  in_flight_buf_ = NULL;
  temp_callback.Run(result_);
}

void FileStream::Context::DeleteOrphanedContext() {
  async_in_progress_ = false;
  callback_.Reset();
  in_flight_buf_ = NULL;
  CloseAndDelete();
}

// static
void FileStream::Context::ReadAsync(
    FileStream::Context* context,
    HANDLE file,
    scoped_refptr<IOBuffer> buf,
    int buf_len,
    OVERLAPPED* overlapped,
    scoped_refptr<base::SingleThreadTaskRunner> origin_thread_task_runner) {
  DWORD bytes_read = 0;
  BOOL ret = ::ReadFile(file, buf->data(), buf_len, &bytes_read, overlapped);
  origin_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&FileStream::Context::ReadAsyncResult,
                 base::Unretained(context), ret, bytes_read, ::GetLastError()));
}

void FileStream::Context::ReadAsyncResult(BOOL read_file_ret,
                                          DWORD bytes_read,
                                          DWORD os_error) {
  // If the context is orphaned and we already received the io completion
  // notification then we should delete the context and get out.
  if (orphaned_ && io_complete_for_read_received_) {
    DeleteOrphanedContext();
    return;
  }

  async_read_completed_ = true;
  if (read_file_ret) {
    result_ = bytes_read;
    InvokeUserCallback();
    return;
  }

  IOResult error = IOResult::FromOSError(os_error);
  if (error.os_error == ERROR_IO_PENDING) {
    InvokeUserCallback();
  } else {
    OnIOCompleted(&io_context_, 0, error.os_error);
  }
}

}  // namespace net
