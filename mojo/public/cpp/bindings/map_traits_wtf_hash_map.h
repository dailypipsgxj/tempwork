// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "third_party/WebKit/Source/wtf/HashMap.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<WTF::HashMap<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename WTF::HashMap<K, V>::iterator;
  using ConstIterator = typename WTF::HashMap<K, V>::const_iterator;

  static bool IsNull(const WTF::HashMap<K, V>& input) {
    // WTF::HashMap<> is always converted to non-null mojom map.
    return false;
  }

  static void SetToNull(WTF::HashMap<K, V>* output) {
    // WTF::HashMap<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const WTF::HashMap<K, V>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const WTF::HashMap<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(WTF::HashMap<K, V>& input) { return input.begin(); }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static void AdvanceIterator(Iterator& iterator) { ++iterator; }

  static const K& GetKey(Iterator& iterator) { return iterator->key; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->key; }

  static V& GetValue(Iterator& iterator) { return iterator->value; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->value; }

  static bool Insert(WTF::HashMap<K, V>& input, const K& key, V&& value) {
    if (!WTF::HashMap<K, V>::isValidKey(key)) {
      LOG(ERROR) << "The key value is disallowed by WTF::HashMap: " << key;
      return false;
    }
    input.add(key, std::forward<V>(value));
    return true;
  }
  static bool Insert(WTF::HashMap<K, V>& input, const K& key, const V& value) {
    if (!WTF::HashMap<K, V>::isValidKey(key)) {
      LOG(ERROR) << "The key value is disallowed by WTF::HashMap: " << key;
      return false;
    }
    input.add(key, value);
    return true;
  }

  static void SetToEmpty(WTF::HashMap<K, V>* output) { output->clear(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_
