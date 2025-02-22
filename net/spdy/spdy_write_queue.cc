// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"

namespace net {

SpdyWriteQueue::PendingWrite::PendingWrite() : frame_producer(NULL) {}

SpdyWriteQueue::PendingWrite::PendingWrite(
    SpdyFrameType frame_type,
    SpdyBufferProducer* frame_producer,
    const base::WeakPtr<SpdyStream>& stream)
    : frame_type(frame_type),
      frame_producer(frame_producer),
      stream(stream),
      has_stream(stream.get() != NULL) {}

SpdyWriteQueue::PendingWrite::PendingWrite(const PendingWrite& other) = default;

SpdyWriteQueue::PendingWrite::~PendingWrite() {}

SpdyWriteQueue::SpdyWriteQueue() : removing_writes_(false) {}

SpdyWriteQueue::~SpdyWriteQueue() {
  Clear();
}

bool SpdyWriteQueue::IsEmpty() const {
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; i++) {
    if (!queue_[i].empty())
      return false;
  }
  return true;
}

void SpdyWriteQueue::Enqueue(RequestPriority priority,
                             SpdyFrameType frame_type,
                             std::unique_ptr<SpdyBufferProducer> frame_producer,
                             const base::WeakPtr<SpdyStream>& stream) {
  CHECK(!removing_writes_);
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);
  if (stream.get())
    DCHECK_EQ(stream->priority(), priority);
  queue_[priority].push_back(
      PendingWrite(frame_type, frame_producer.release(), stream));
}

bool SpdyWriteQueue::Dequeue(
    SpdyFrameType* frame_type,
    std::unique_ptr<SpdyBufferProducer>* frame_producer,
    base::WeakPtr<SpdyStream>* stream) {
  CHECK(!removing_writes_);
  for (int i = MAXIMUM_PRIORITY; i >= MINIMUM_PRIORITY; --i) {
    if (!queue_[i].empty()) {
      PendingWrite pending_write = queue_[i].front();
      queue_[i].pop_front();
      *frame_type = pending_write.frame_type;
      frame_producer->reset(pending_write.frame_producer);
      *stream = pending_write.stream;
      if (pending_write.has_stream)
        DCHECK(stream->get());
      return true;
    }
  }
  return false;
}

void SpdyWriteQueue::RemovePendingWritesForStream(
    const base::WeakPtr<SpdyStream>& stream) {
  CHECK(!removing_writes_);
  removing_writes_ = true;
  RequestPriority priority = stream->priority();
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);

  DCHECK(stream.get());
#if DCHECK_IS_ON()
  // |stream| should not have pending writes in a queue not matching
  // its priority.
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    if (priority == i)
      continue;
    for (std::deque<PendingWrite>::const_iterator it = queue_[i].begin();
         it != queue_[i].end(); ++it) {
      DCHECK_NE(it->stream.get(), stream.get());
    }
  }
#endif

  // Defer deletion until queue iteration is complete, as
  // SpdyBuffer::~SpdyBuffer() can result in callbacks into SpdyWriteQueue.
  std::vector<SpdyBufferProducer*> erased_buffer_producers;

  // Do the actual deletion and removal, preserving FIFO-ness.
  std::deque<PendingWrite>* queue = &queue_[priority];
  std::deque<PendingWrite>::iterator out_it = queue->begin();
  for (std::deque<PendingWrite>::const_iterator it = queue->begin();
       it != queue->end(); ++it) {
    if (it->stream.get() == stream.get()) {
      erased_buffer_producers.push_back(it->frame_producer);
    } else {
      *out_it = *it;
      ++out_it;
    }
  }
  queue->erase(out_it, queue->end());
  removing_writes_ = false;
  STLDeleteElements(&erased_buffer_producers);  // Invokes callbacks.
}

void SpdyWriteQueue::RemovePendingWritesForStreamsAfter(
    SpdyStreamId last_good_stream_id) {
  CHECK(!removing_writes_);
  removing_writes_ = true;
  std::vector<SpdyBufferProducer*> erased_buffer_producers;

  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    // Do the actual deletion and removal, preserving FIFO-ness.
    std::deque<PendingWrite>* queue = &queue_[i];
    std::deque<PendingWrite>::iterator out_it = queue->begin();
    for (std::deque<PendingWrite>::const_iterator it = queue->begin();
         it != queue->end(); ++it) {
      if (it->stream.get() && (it->stream->stream_id() > last_good_stream_id ||
                               it->stream->stream_id() == 0)) {
        erased_buffer_producers.push_back(it->frame_producer);
      } else {
        *out_it = *it;
        ++out_it;
      }
    }
    queue->erase(out_it, queue->end());
  }
  removing_writes_ = false;
  STLDeleteElements(&erased_buffer_producers);  // Invokes callbacks.
}

void SpdyWriteQueue::Clear() {
  CHECK(!removing_writes_);
  removing_writes_ = true;
  std::vector<SpdyBufferProducer*> erased_buffer_producers;

  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    for (std::deque<PendingWrite>::iterator it = queue_[i].begin();
         it != queue_[i].end(); ++it) {
      erased_buffer_producers.push_back(it->frame_producer);
    }
    queue_[i].clear();
  }
  removing_writes_ = false;
  STLDeleteElements(&erased_buffer_producers);  // Invokes callbacks.
}

}  // namespace net
