// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_

#include <new>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {
namespace internal {

template <typename Struct>
class StructHelper {
 public:
  template <typename Ptr>
  static void Initialize(Ptr* ptr) {
    ptr->Initialize();
  }
};

}  // namespace internal

// Smart pointer wrapping a mojom structure with move-only semantics.
template <typename S>
class StructPtr {
 public:
  using Struct = S;

  StructPtr() : ptr_(nullptr) {}
  StructPtr(decltype(nullptr)) : ptr_(nullptr) {}

  ~StructPtr() { delete ptr_; }

  StructPtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  StructPtr(StructPtr&& other) : ptr_(nullptr) { Take(&other); }
  StructPtr& operator=(StructPtr&& other) {
    Take(&other);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, StructPtr>::Convert(*this);
  }

  void reset() {
    if (ptr_) {
      delete ptr_;
      ptr_ = nullptr;
    }
  }

  bool is_null() const { return ptr_ == nullptr; }

  Struct& operator*() const {
    DCHECK(ptr_);
    return *ptr_;
  }
  Struct* operator->() const {
    DCHECK(ptr_);
    return ptr_;
  }
  Struct* get() const { return ptr_; }

  void Swap(StructPtr* other) { std::swap(ptr_, other->ptr_); }

  // Please note that calling this method will fail compilation if the value
  // type |Struct| doesn't have a Clone() method defined (which usually means
  // that it contains Mojo handles).
  StructPtr Clone() const { return is_null() ? StructPtr() : ptr_->Clone(); }

  bool Equals(const StructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return ptr_->Equals(*other.ptr_);
  }

 private:
  // TODO(dcheng): Use an explicit conversion operator.
  typedef Struct* StructPtr::*Testable;

 public:
  operator Testable() const { return ptr_ ? &StructPtr::ptr_ : 0; }

 private:
  friend class internal::StructHelper<Struct>;

  // Forbid the == and != operators explicitly, otherwise StructPtr will be
  // converted to Testable to do == or != comparison.
  template <typename T>
  bool operator==(const StructPtr<T>& other) const = delete;
  template <typename T>
  bool operator!=(const StructPtr<T>& other) const = delete;

  void Initialize() {
    DCHECK(!ptr_);
    ptr_ = new Struct();
  }

  void Take(StructPtr* other) {
    reset();
    Swap(other);
  }

  Struct* ptr_;

  DISALLOW_COPY_AND_ASSIGN(StructPtr);
};

// Designed to be used when Struct is small and copyable.
template <typename S>
class InlinedStructPtr {
 public:
  using Struct = S;

  InlinedStructPtr() : is_null_(true) {}
  InlinedStructPtr(decltype(nullptr)) : is_null_(true) {}

  ~InlinedStructPtr() {}

  InlinedStructPtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  InlinedStructPtr(InlinedStructPtr&& other) : is_null_(true) { Take(&other); }
  InlinedStructPtr& operator=(InlinedStructPtr&& other) {
    Take(&other);
    return *this;
  }

  template <typename U>
  U To() const {
    return TypeConverter<U, InlinedStructPtr>::Convert(*this);
  }

  void reset() {
    is_null_ = true;
    value_. ~Struct();
    new (&value_) Struct();
  }

  bool is_null() const { return is_null_; }

  Struct& operator*() const {
    DCHECK(!is_null_);
    return value_;
  }
  Struct* operator->() const {
    DCHECK(!is_null_);
    return &value_;
  }
  Struct* get() const { return &value_; }

  void Swap(InlinedStructPtr* other) {
    std::swap(value_, other->value_);
    std::swap(is_null_, other->is_null_);
  }

  InlinedStructPtr Clone() const {
    return is_null() ? InlinedStructPtr() : value_.Clone();
  }
  bool Equals(const InlinedStructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return value_.Equals(other.value_);
  }

 private:
  // TODO(dcheng): Use an explicit conversion operator.
  typedef Struct InlinedStructPtr::*Testable;

 public:
  operator Testable() const { return is_null_ ? 0 : &InlinedStructPtr::value_; }

 private:
  friend class internal::StructHelper<Struct>;

  // Forbid the == and != operators explicitly, otherwise InlinedStructPtr will
  // be converted to Testable to do == or != comparison.
  template <typename T>
  bool operator==(const InlinedStructPtr<T>& other) const = delete;
  template <typename T>
  bool operator!=(const InlinedStructPtr<T>& other) const = delete;

  void Initialize() { is_null_ = false; }

  void Take(InlinedStructPtr* other) {
    reset();
    Swap(other);
  }

  mutable Struct value_;
  bool is_null_;

  DISALLOW_COPY_AND_ASSIGN(InlinedStructPtr);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
