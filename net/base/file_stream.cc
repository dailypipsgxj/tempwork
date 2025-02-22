// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include <utility>

#include "base/profiler/scoped_tracker.h"
#include "net/base/file_stream_context.h"
#include "net/base/net_errors.h"

namespace net {

FileStream::FileStream(const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(new Context(task_runner)) {
}

FileStream::FileStream(base::File file,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(new Context(std::move(file), task_runner)) {}

FileStream::~FileStream() {
  context_.release()->Orphan();
}

int FileStream::Open(const base::FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(open_flags & base::File::FLAG_ASYNC);
  context_->Open(path, open_flags, callback);
  return ERR_IO_PENDING;
}

int FileStream::Close(const CompletionCallback& callback) {
  context_->Close(callback);
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->IsOpen();
}

int FileStream::Seek(int64_t offset, const Int64CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Seek(offset, callback);
  return ERR_IO_PENDING;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) {
  // TODO(rvargas): Remove ScopedTracker below once crbug.com/475751 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("475751 FileStream::Read"));

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->Read(buf, buf_len, callback);
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK_GE(buf_len, 0);
  return context_->Write(buf, buf_len, callback);
}

int FileStream::Flush(const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Flush(callback);
  return ERR_IO_PENDING;
}

}  // namespace net
