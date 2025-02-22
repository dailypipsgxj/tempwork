// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/base/upload_element_reader.h"

namespace net {

// An UploadElementReader implementation for bytes. The caller owns |bytes|,
// and is responsible for ensuring it outlives the UploadBytesElementReader.
class NET_EXPORT UploadBytesElementReader : public UploadElementReader {
 public:
  UploadBytesElementReader(const char* bytes, uint64_t length);
  ~UploadBytesElementReader() override;

  const char* bytes() const { return bytes_; }
  uint64_t length() const { return length_; }

  // UploadElementReader overrides:
  const UploadBytesElementReader* AsBytesReader() const override;
  int Init(const CompletionCallback& callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  bool IsInMemory() const override;
  int Read(IOBuffer* buf,
           int buf_length,
           const CompletionCallback& callback) override;

 private:
  const char* const bytes_;
  const uint64_t length_;
  uint64_t offset_;

  DISALLOW_COPY_AND_ASSIGN(UploadBytesElementReader);
};

// A subclass of UplodBytesElementReader which owns the data given as a vector.
class NET_EXPORT UploadOwnedBytesElementReader
    : public UploadBytesElementReader {
 public:
  // |data| is cleared by this ctor.
  explicit UploadOwnedBytesElementReader(std::vector<char>* data);
  ~UploadOwnedBytesElementReader() override;

  // Creates UploadOwnedBytesElementReader with a string.
  static UploadOwnedBytesElementReader* CreateWithString(
      const std::string& string);

 private:
  std::vector<char> data_;

  DISALLOW_COPY_AND_ASSIGN(UploadOwnedBytesElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
