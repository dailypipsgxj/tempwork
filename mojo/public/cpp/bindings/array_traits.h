// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom array.
//
// Usually you would like to do a partial specialization for a container (e.g.
// vector) template. Imagine you want to specialize it for Container<>, you need
// to implement:
//
//   template <typename T>
//   struct ArrayTraits<Container<T>> {
//     using Element = T;
//     // These two statements are optional. Use them if you'd like to serialize
//     // a container that supports iterators but does not support O(1) random
//     // access and so GetAt(...) would be expensive.
//     // using Iterator = T::iterator;
//     // using ConstIterator = T::const_iterator;
//
//     // These two methods are optional. Please see comments in struct_traits.h
//     static bool IsNull(const Container<T>& input);
//     static void SetToNull(Container<T>* output);
//
//     static size_t GetSize(const Container<T>& input);
//
//     // These two methods are optional. They are used to access the
//     // underlying storage of the array to speed up copy of POD types.
//     static T* GetData(Container<T>& input);
//     static const T* GetData(const Container<T>& input);
//
//     // The following six methods are optional if the GetAt(...) methods are
//     // implemented. These methods specify how to read the elements of
//     // Container in some sequential order specified by the iterator.
//     //
//     // Acquires an iterator positioned at the first element in the container.
//     static ConstIterator GetBegin(const Container<T>& input);
//     static Iterator GetBegin(Container<T>& input);
//
//     // Advances |iterator| to the next position within the container.
//     static void AdvanceIterator(ConstIterator& iterator);
//     static void AdvanceIterator(Iterator& iterator);
//
//     // Returns a reference to the value at the current position of
//     // |iterator|. Optionally, the ConstIterator version of GetValue can
//     // return by value instead of by reference if it makes sense for the
//     // type.
//     static const T& GetValue(ConstIterator& iterator);
//     static T& GetValue(Iterator& iterator);
//
//     // These two methods are optional if the iterator methods are
//     // implemented.
//     static T& GetAt(Container<T>& input, size_t index);
//     static const T& GetAt(const Container<T>& input, size_t index);
//
//     // Returning false results in deserialization failure and causes the
//     // message pipe receiving it to be disconnected.
//     static bool Resize(Container<T>& input, size_t size);
//   };
//
template <typename T>
struct ArrayTraits;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
