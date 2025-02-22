// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STL_H_

#include <map>
#include <unordered_map>

#include "mojo/public/cpp/bindings/map_traits.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<std::map<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename std::map<K, V>::iterator;
  using ConstIterator = typename std::map<K, V>::const_iterator;

  static bool IsNull(const std::map<K, V>& input) {
    // std::map<> is always converted to non-null mojom map.
    return false;
  }

  static void SetToNull(std::map<K, V>* output) {
    // std::map<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const std::map<K, V>& input) { return input.size(); }

  static ConstIterator GetBegin(const std::map<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(std::map<K, V>& input) { return input.begin(); }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const K& GetKey(Iterator& iterator) { return iterator->first; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->first; }

  static V& GetValue(Iterator& iterator) { return iterator->second; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }

  static bool Insert(std::map<K, V>& input, const K& key, V&& value) {
    input.insert(std::make_pair(key, std::forward<V>(value)));
    return true;
  }
  static bool Insert(std::map<K, V>& input, const K& key, const V& value) {
    input.insert(std::make_pair(key, value));
    return true;
  }

  static void SetToEmpty(std::map<K, V>* output) { output->clear(); }
};

template <typename K, typename V>
struct MapTraits<std::unordered_map<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename std::unordered_map<K, V>::iterator;
  using ConstIterator = typename std::unordered_map<K, V>::const_iterator;

  static bool IsNull(const std::unordered_map<K, V>& input) {
    // std::unordered_map<> is always converted to non-null mojom map.
    return false;
  }

  static void SetToNull(std::unordered_map<K, V>* output) {
    // std::unordered_map<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const std::unordered_map<K, V>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const std::unordered_map<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(std::unordered_map<K, V>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const K& GetKey(Iterator& iterator) { return iterator->first; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->first; }

  static V& GetValue(Iterator& iterator) { return iterator->second; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }

  static bool Insert(std::unordered_map<K, V>& input, const K& key, V&& value) {
    input.insert(std::make_pair(key, std::forward<V>(value)));
    return true;
  }
  static bool Insert(std::unordered_map<K, V>& input,
                     const K& key,
                     const V& value) {
    input.insert(std::make_pair(key, value));
    return true;
  }

  static void SetToEmpty(std::unordered_map<K, V>* output) { output->clear(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_STL_H_
