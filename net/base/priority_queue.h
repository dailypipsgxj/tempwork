// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIORITY_QUEUE_H_
#define NET_BASE_PRIORITY_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"

#if !defined(NDEBUG)
#include <unordered_set>
#endif

namespace net {

// A simple priority queue. The order of values is by priority and then FIFO.
// Unlike the std::priority_queue, this implementation allows erasing elements
// from the queue, and all operations are O(p) time for p priority levels.
// The queue is agnostic to priority ordering (whether 0 precedes 1).
// If the highest priority is 0, FirstMin() returns the first in order.
//
// In debug-mode, the internal queues store (id, value) pairs where id is used
// to validate Pointers.
//
template<typename T>
class PriorityQueue : public base::NonThreadSafe {
 private:
  // This section is up-front for Pointer only.
#if !defined(NDEBUG)
  typedef std::list<std::pair<unsigned, T> > List;
#else
  typedef std::list<T> List;
#endif

 public:
  typedef uint32_t Priority;

  // A pointer to a value stored in the queue. The pointer becomes invalid
  // when the queue is destroyed or cleared, or the value is erased.
  class Pointer {
   public:
    // Constructs a null pointer.
    Pointer() : priority_(kNullPriority) {
#if !defined(NDEBUG)
      id_ = static_cast<unsigned>(-1);
#endif
      // TODO(syzm)
      // An uninitialized iterator behaves like an uninitialized pointer as per
      // the STL docs. The fix below is ugly and should possibly be replaced
      // with a better approach.
      iterator_ = dummy_empty_list_.end();
    }

    Pointer(const Pointer& p)
        : priority_(p.priority_),
          iterator_(p.iterator_) {
#if !defined(NDEBUG)
      id_ = p.id_;
#endif
    }

    Pointer& operator=(const Pointer& p) {
      // Self-assignment is benign.
      priority_ = p.priority_;
      iterator_ = p.iterator_;
#if !defined(NDEBUG)
      id_ = p.id_;
#endif
      return *this;
    }

    bool is_null() const { return priority_ == kNullPriority; }

    Priority priority() const { return priority_; }

#if !defined(NDEBUG)
    const T& value() const { return iterator_->second; }
#else
    const T& value() const { return *iterator_; }
#endif

    // Comparing to Pointer from a different PriorityQueue is undefined.
    bool Equals(const Pointer& other) const {
      return (priority_ == other.priority_) && (iterator_ == other.iterator_);
    }

    void Reset() {
      *this = Pointer();
    }

   private:
    friend class PriorityQueue;

    // Note that we need iterator and not const_iterator to pass to
    // List::erase.  When C++11 is turned on for Chromium, this could
    // be changed to const_iterator and the const_casts in the rest of
    // the file can be removed.
    typedef typename PriorityQueue::List::iterator ListIterator;

    static const Priority kNullPriority = static_cast<Priority>(-1);

    // It is guaranteed that Pointer will treat |iterator| as a
    // const_iterator.
    Pointer(Priority priority, const ListIterator& iterator)
        : priority_(priority), iterator_(iterator) {
#if !defined(NDEBUG)
      id_ = iterator_->first;
#endif
    }

    Priority priority_;
    ListIterator iterator_;
    // The STL iterators when uninitialized are like uninitialized pointers
    // which cause crashes when assigned to other iterators. We need to
    // initialize a NULL iterator to the end of a valid list.
    List dummy_empty_list_;

#if !defined(NDEBUG)
    // Used by the queue to check if a Pointer is valid.
    unsigned id_;
#endif
  };

  // Creates a new queue for |num_priorities|.
  explicit PriorityQueue(Priority num_priorities)
      : lists_(num_priorities), size_(0) {
#if !defined(NDEBUG)
    next_id_ = 0;
#endif
  }

  // Adds |value| with |priority| to the queue. Returns a pointer to the
  // created element.
  Pointer Insert(const T& value, Priority priority) {
    DCHECK(CalledOnValidThread());
    DCHECK_LT(priority, lists_.size());
    ++size_;
    List& list = lists_[priority];
#if !defined(NDEBUG)
    unsigned id = next_id_;
    valid_ids_.insert(id);
    ++next_id_;
    return Pointer(priority, list.insert(list.end(),
                                         std::make_pair(id, value)));
#else
    return Pointer(priority, list.insert(list.end(), value));
#endif
  }

  // Adds |value| with |priority| to the queue. Returns a pointer to the
  // created element.
  Pointer InsertAtFront(const T& value, Priority priority) {
    DCHECK(CalledOnValidThread());
    DCHECK_LT(priority, lists_.size());
    ++size_;
    List& list = lists_[priority];
#if !defined(NDEBUG)
    unsigned id = next_id_;
    valid_ids_.insert(id);
    ++next_id_;
    return Pointer(priority, list.insert(list.begin(),
                                         std::make_pair(id, value)));
#else
    return Pointer(priority, list.insert(list.begin(), value));
#endif
  }

  // Removes the value pointed by |pointer| from the queue. All pointers to this
  // value including |pointer| become invalid.
  void Erase(const Pointer& pointer) {
    DCHECK(CalledOnValidThread());
    DCHECK_LT(pointer.priority_, lists_.size());
    DCHECK_GT(size_, 0u);

#if !defined(NDEBUG)
    DCHECK_EQ(1u, valid_ids_.erase(pointer.id_));
    DCHECK_EQ(pointer.iterator_->first, pointer.id_);
#endif

    --size_;
    lists_[pointer.priority_].erase(pointer.iterator_);
  }

  // Returns a pointer to the first value of minimum priority or a null-pointer
  // if empty.
  Pointer FirstMin() const {
    DCHECK(CalledOnValidThread());
    for (size_t i = 0; i < lists_.size(); ++i) {
      List* list = const_cast<List*>(&lists_[i]);
      if (!list->empty())
        return Pointer(i, list->begin());
    }
    return Pointer();
  }

  // Returns a pointer to the last value of minimum priority or a null-pointer
  // if empty.
  Pointer LastMin() const {
    DCHECK(CalledOnValidThread());
    for (size_t i = 0; i < lists_.size(); ++i) {
      List* list = const_cast<List*>(&lists_[i]);
      if (!list->empty())
        return Pointer(i, --list->end());
    }
    return Pointer();
  }

  // Returns a pointer to the first value of maximum priority or a null-pointer
  // if empty.
  Pointer FirstMax() const {
    DCHECK(CalledOnValidThread());
    for (size_t i = lists_.size(); i > 0; --i) {
      size_t index = i - 1;
      List* list = const_cast<List*>(&lists_[index]);
      if (!list->empty())
        return Pointer(index, list->begin());
    }
    return Pointer();
  }

  // Returns a pointer to the last value of maximum priority or a null-pointer
  // if empty.
  Pointer LastMax() const {
    DCHECK(CalledOnValidThread());
    for (size_t i = lists_.size(); i > 0; --i) {
      size_t index = i - 1;
      List* list = const_cast<List*>(&lists_[index]);
      if (!list->empty())
        return Pointer(index, --list->end());
    }
    return Pointer();
  }

  // Given an ordering of the values in this queue by decreasing
  // priority and then FIFO, returns a pointer to the value following
  // the value of the given pointer (which must be non-NULL).
  //
  // (One could also implement GetNextTowardsFirstMin() [decreasing
  // priority, then reverse FIFO] as well as
  // GetNextTowards{First,Last}Max() [increasing priority, then
  // {,reverse} FIFO].)
  Pointer GetNextTowardsLastMin(const Pointer& pointer) const {
    DCHECK(CalledOnValidThread());
    DCHECK(!pointer.is_null());
    DCHECK_LT(pointer.priority_, lists_.size());

    typename Pointer::ListIterator it = pointer.iterator_;
    Priority priority = pointer.priority_;
    DCHECK(it != lists_[priority].end());
    ++it;
    while (it == lists_[priority].end()) {
      if (priority == 0u)
        return Pointer();
      --priority;
      it = const_cast<List*>(&lists_[priority])->begin();
    }
    return Pointer(priority, it);
  }

  // Empties the queue. All pointers become invalid.
  void Clear() {
    DCHECK(CalledOnValidThread());
    for (size_t i = 0; i < lists_.size(); ++i) {
      lists_[i].clear();
    }
#if !defined(NDEBUG)
    valid_ids_.clear();
#endif
    size_ = 0u;
  }

  // Returns the number of priorities the queue supports.
  size_t num_priorities() const {
    DCHECK(CalledOnValidThread());
    return lists_.size();
  }

  bool empty() const {
    DCHECK(CalledOnValidThread());
    return size_ == 0;
  }

  // Returns number of queued values.
  size_t size() const {
    DCHECK(CalledOnValidThread());
    return size_;
  }

 private:
  typedef std::vector<List> ListVector;

#if !defined(NDEBUG)
  unsigned next_id_;
  std::unordered_set<unsigned> valid_ids_;
#endif

  ListVector lists_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(PriorityQueue);
};

}  // namespace net

#endif  // NET_BASE_PRIORITY_QUEUE_H_
