// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"

namespace mojo {
namespace internal {

template <typename T, typename EnableType = void>
class ArrayDataViewImpl;

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<BelongsTo<typename DataViewTraits<T>::MojomType,
                                      MojomTypeCategory::POD>::value>::type> {
 public:
  static_assert(std::is_same<T, typename DataViewTraits<T>::MojomType>::value,
                "DataView type mismatch");

  using Data_ = typename MojomTypeTraits<Array<T>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  T operator[](size_t index) const { return data_->at(index); }

  const T* data() const { return data_->storage(); }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

template <typename T>
class ArrayDataViewImpl<T,
                        typename std::enable_if<BelongsTo<
                            typename DataViewTraits<T>::MojomType,
                            MojomTypeCategory::BOOLEAN>::value>::type> {
 public:
  static_assert(std::is_same<T, typename DataViewTraits<T>::MojomType>::value,
                "DataView type mismatch");

  using Data_ = typename MojomTypeTraits<Array<T>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  bool operator[](size_t index) const { return data_->at(index); }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<BelongsTo<typename DataViewTraits<T>::MojomType,
                                      MojomTypeCategory::ENUM>::value>::type> {
 public:
  static_assert(std::is_same<T, typename DataViewTraits<T>::MojomType>::value,
                "DataView type mismatch");
  static_assert(sizeof(T) == sizeof(int32_t), "Unexpected enum size");

  using Data_ = typename MojomTypeTraits<Array<T>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  T operator[](size_t index) const { return static_cast<T>(data_->at(index)); }

  const T* data() const { return reinterpret_cast<const T*>(data_->storage()); }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<T>(data_->at(index), output);
  }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<
        BelongsTo<typename DataViewTraits<T>::MojomType,
                  MojomTypeCategory::ASSOCIATED_INTERFACE |
                      MojomTypeCategory::ASSOCIATED_INTERFACE_REQUEST |
                      MojomTypeCategory::HANDLE |
                      MojomTypeCategory::INTERFACE |
                      MojomTypeCategory::INTERFACE_REQUEST>::value>::type> {
 public:
  static_assert(std::is_same<T, typename DataViewTraits<T>::MojomType>::value,
                "DataView type mismatch");

  using Data_ = typename MojomTypeTraits<Array<T>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  T Take(size_t index) {
    T result;
    bool ret = Deserialize<T>(&data_->at(index), &result, context_);
    DCHECK(ret);
    return result;
  }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

template <typename T>
class ArrayDataViewImpl<T,
                        typename std::enable_if<BelongsTo<
                            typename DataViewTraits<T>::MojomType,
                            MojomTypeCategory::ARRAY | MojomTypeCategory::MAP |
                                MojomTypeCategory::STRING |
                                MojomTypeCategory::STRUCT>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<
      Array<typename DataViewTraits<T>::MojomType>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  void GetDataView(size_t index, T* output) {
    *output = T(data_->at(index).Get(), context_);
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<typename DataViewTraits<T>::MojomType>(
        data_->at(index).Get(), output, context_);
  }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

template <typename T>
class ArrayDataViewImpl<
    T,
    typename std::enable_if<BelongsTo<typename DataViewTraits<T>::MojomType,
                                      MojomTypeCategory::UNION>::value>::type> {
 public:
  using Data_ = typename MojomTypeTraits<
      Array<typename DataViewTraits<T>::MojomType>>::Data;

  ArrayDataViewImpl(Data_* data, SerializationContext* context)
      : data_(data), context_(context) {}

  void GetDataView(size_t index, T* output) {
    *output = T(&data_->at(index), context_);
  }

  template <typename U>
  bool Read(size_t index, U* output) {
    return Deserialize<typename DataViewTraits<T>::MojomType>(&data_->at(index),
                                                              output, context_);
  }

 protected:
  Data_* data_;
  SerializationContext* context_;
};

}  // namespace internal

template <typename K, typename V>
class MapDataView;

template <typename T>
class ArrayDataView : public internal::ArrayDataViewImpl<T> {
 public:
  using Data_ = typename internal::ArrayDataViewImpl<T>::Data_;

  ArrayDataView() : internal::ArrayDataViewImpl<T>(nullptr, nullptr) {}

  ArrayDataView(Data_* data, internal::SerializationContext* context)
      : internal::ArrayDataViewImpl<T>(data, context) {}

  bool is_null() const { return !this->data_; }

  size_t size() const { return this->data_->size(); }

  // Methods to access elements are different for different element types. They
  // are inherited from internal::ArrayDataViewImpl:

  // POD types except boolean and enums:
  //   T operator[](size_t index) const;
  //   const T* data() const;

  // Boolean:
  //   bool operator[](size_t index) const;

  // Enums:
  //   T operator[](size_t index) const;
  //   const T* data() const;
  //   template <typename U>
  //   bool Read(size_t index, U* output);

  // Handles and interfaces:
  //   T Take(size_t index);

  // Object types:
  //   void GetDataView(size_t index, T* output);
  //   template <typename U>
  //   bool Read(size_t index, U* output);

 private:
  template <typename K, typename V>
  friend class MapDataView;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_DATA_VIEW_H_
