// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a set of root CAs which are required to disclose
// all certificates via Certificate Transparency, as well as exceptions
// for independent and disclosed sub-CAs.
//
// It is meant to be directly included in transport_security_state.cc
// within an unnamed namespace.

struct CTRequiredPolicy {
  // A certificate MUST be disclosed via Certificate Transparency if it
  // chains to or through one of the values contained in |roots|, which
  // contains the SHA-256 hash of the issuing CA's SubjectPublicKeyInfo,
  // the same format as HTTP Public Key Pinning.
  const SHA256HashValue* roots;

  // The number of entries in |roots|.
  size_t roots_length;

  // The date at which enforcement should begin, relative to the Unix
  // Epoch.
  base::TimeDelta effective_date;

  // However, if a certificate ALSO chains to or through one of
  // |exceptions|, which also contains the SHA-256 hashes of the
  // issuing CA's SubjectPublicKeyInfo, then even though it chained
  // through |roots|, it will be exempt from CT requirements.
  const SHA256HashValue* exceptions;

  // The number of entries in |exceptions|.
  size_t exceptions_length;
};

// See net/data/ssl/symantec/README.md
const SHA256HashValue kSymantecRoots[] = {
    {{0x02, 0x3c, 0x81, 0xcc, 0xe8, 0xe7, 0xc6, 0x4f, 0xa9, 0x42, 0xd3,
      0xc1, 0x50, 0x48, 0x70, 0x7d, 0x35, 0xd9, 0xbb, 0x5b, 0x87, 0xf4,
      0xf5, 0x44, 0xc5, 0xbf, 0x1b, 0xc5, 0x64, 0x3a, 0xf2, 0xfa}},
    {{0x09, 0x99, 0xbf, 0x90, 0x0b, 0xd5, 0xc2, 0x97, 0x86, 0x5e, 0x21,
      0xe1, 0xaa, 0xde, 0x6c, 0xf6, 0xbb, 0x3a, 0x94, 0xd1, 0x1a, 0xe5,
      0xea, 0x79, 0x84, 0x42, 0xa4, 0xe2, 0xf8, 0x13, 0x24, 0x1f}},
    {{0x0b, 0xdd, 0x5a, 0xbe, 0x94, 0x0c, 0xaa, 0xab, 0xe8, 0xb2, 0xbb,
      0xa8, 0x83, 0x48, 0xfb, 0x6f, 0x4a, 0xa4, 0xcc, 0x84, 0x43, 0x6f,
      0x88, 0x0b, 0xec, 0xe6, 0x6b, 0x48, 0xbd, 0xa9, 0x13, 0xd8}},
    {{0x16, 0xa9, 0xe0, 0x12, 0xd3, 0x23, 0x29, 0xf2, 0x82, 0xb1, 0x0b,
      0xbf, 0x57, 0xc7, 0xc0, 0xb4, 0x2a, 0xe8, 0x0f, 0x6a, 0xc9, 0x54,
      0x2e, 0xb4, 0x09, 0xbc, 0x1c, 0x2c, 0xde, 0x50, 0xd3, 0x22}},
    {{0x17, 0x75, 0x5a, 0x5c, 0x29, 0x5f, 0x3d, 0x2d, 0x72, 0xe6, 0xf0,
      0x31, 0xa1, 0xf0, 0x7f, 0x40, 0x0c, 0x58, 0x8b, 0x9e, 0x58, 0x2b,
      0x22, 0xf1, 0x7e, 0xae, 0x31, 0xa1, 0x59, 0x0d, 0x11, 0x85}},
    {{0x19, 0x06, 0xc6, 0x12, 0x4d, 0xbb, 0x43, 0x85, 0x78, 0xd0, 0x0e,
      0x06, 0x6d, 0x50, 0x54, 0xc6, 0xc3, 0x7f, 0x0f, 0xa6, 0x02, 0x8c,
      0x05, 0x54, 0x5e, 0x09, 0x94, 0xed, 0xda, 0xec, 0x86, 0x29}},
    {{0x19, 0x16, 0xf3, 0x50, 0x8e, 0xc3, 0xfa, 0xd7, 0x95, 0xf8, 0xdc,
      0x4b, 0xd3, 0x16, 0xf9, 0xc6, 0x08, 0x5a, 0x64, 0xde, 0x3c, 0x41,
      0x53, 0xac, 0x6d, 0x62, 0xd5, 0xea, 0x19, 0x51, 0x5d, 0x39}},
    {{0x1d, 0x75, 0xd0, 0x83, 0x1b, 0x9e, 0x08, 0x85, 0x39, 0x4d, 0x32,
      0xc7, 0xa1, 0xbf, 0xdb, 0x3d, 0xbc, 0x1c, 0x28, 0xe2, 0xb0, 0xe8,
      0x39, 0x1f, 0xb1, 0x35, 0x98, 0x1d, 0xbc, 0x5b, 0xa9, 0x36}},
    {{0x22, 0x07, 0x6e, 0x5a, 0xef, 0x44, 0xbb, 0x9a, 0x41, 0x6a, 0x28,
      0xb7, 0xd1, 0xc4, 0x43, 0x22, 0xd7, 0x05, 0x9f, 0x60, 0xfe, 0xff,
      0xa5, 0xca, 0xf6, 0xc5, 0xbe, 0x84, 0x47, 0x89, 0x13, 0x03}},
    {{0x25, 0xb4, 0x1b, 0x50, 0x6e, 0x49, 0x30, 0x95, 0x28, 0x23, 0xa6,
      0xeb, 0x9f, 0x1d, 0x31, 0xde, 0xf6, 0x45, 0xea, 0x38, 0xa5, 0xc6,
      0xc6, 0xa9, 0x6d, 0x71, 0x95, 0x7e, 0x38, 0x4d, 0xf0, 0x58}},
    {{0x26, 0xc1, 0x8d, 0xc6, 0xee, 0xa6, 0xf6, 0x32, 0xf6, 0x76, 0xbc,
      0xeb, 0xa1, 0xd8, 0xc2, 0xb4, 0x83, 0x52, 0xf2, 0x9c, 0x2d, 0x5f,
      0xcd, 0xa8, 0x78, 0xe0, 0x9d, 0xcb, 0x83, 0x2d, 0xd6, 0xe5}},
    {{0x2d, 0xc9, 0x47, 0x0b, 0xe6, 0x3e, 0xf4, 0xac, 0xf1, 0xbd, 0x82,
      0x86, 0x09, 0x40, 0x2b, 0xb7, 0xb8, 0x7b, 0xd9, 0x96, 0x38, 0xa6,
      0x43, 0x93, 0x4e, 0x88, 0x68, 0x2d, 0x1b, 0xe8, 0xc3, 0x08}},
    {{0x2d, 0xee, 0x51, 0x71, 0x59, 0x6a, 0xb8, 0xf3, 0xcd, 0x3c, 0x76,
      0x35, 0xfe, 0xa8, 0xe6, 0xc3, 0x00, 0x6a, 0xa9, 0xe3, 0x1d, 0xb3,
      0x9d, 0x03, 0xa7, 0x48, 0x0d, 0xdb, 0x24, 0x28, 0xa3, 0x3e}},
    {{0x30, 0x27, 0xa2, 0x98, 0xfa, 0x57, 0x31, 0x4d, 0xc0, 0xe3, 0xdd,
      0x10, 0x19, 0x41, 0x1b, 0x8f, 0x40, 0x4c, 0x43, 0xc3, 0xf9, 0x34,
      0xce, 0x3b, 0xdf, 0x85, 0x65, 0x12, 0xc8, 0x0a, 0xa1, 0x5c}},
    {{0x31, 0x51, 0x26, 0x80, 0x23, 0x3f, 0x5f, 0x2a, 0x1f, 0x29, 0x43,
      0x7f, 0x56, 0xd4, 0x98, 0x8c, 0xf0, 0xaf, 0xc4, 0x1c, 0xc6, 0xc5,
      0xda, 0x62, 0x75, 0x92, 0x8e, 0x9c, 0x0b, 0xea, 0xde, 0x27}},
    {{0x43, 0xb3, 0x10, 0x7d, 0x73, 0x42, 0x16, 0x5d, 0x40, 0x6c, 0xf9,
      0x75, 0xcd, 0x79, 0xb3, 0x6e, 0xd1, 0x64, 0x50, 0x48, 0xf0, 0x5d,
      0x7f, 0xf6, 0xea, 0x00, 0x96, 0xe4, 0x27, 0xb7, 0xdb, 0x84}},
    {{0x46, 0x3d, 0xbb, 0x9b, 0x0a, 0x26, 0xed, 0x26, 0x16, 0x39, 0x7b,
      0x64, 0x31, 0x25, 0xfb, 0xd2, 0x9b, 0x66, 0xcf, 0x3a, 0x46, 0xfd,
      0xb4, 0x38, 0x4b, 0x20, 0x9e, 0x78, 0x23, 0x7a, 0x1a, 0xff}},
    {{0x47, 0x9d, 0x13, 0x0b, 0xf3, 0xfc, 0x61, 0xdc, 0x2f, 0x1d, 0x50,
      0x8d, 0x23, 0x9a, 0x13, 0x27, 0x6a, 0xe7, 0xb3, 0xc9, 0x84, 0x10,
      0x11, 0xa0, 0x2c, 0x14, 0x02, 0xc7, 0xe6, 0x77, 0xbd, 0x5f}},
    {{0x49, 0x05, 0x46, 0x66, 0x23, 0xab, 0x41, 0x78, 0xbe, 0x92, 0xac,
      0x5c, 0xbd, 0x65, 0x84, 0xf7, 0xa1, 0xe1, 0x7f, 0x27, 0x65, 0x2d,
      0x5a, 0x85, 0xaf, 0x89, 0x50, 0x4e, 0xa2, 0x39, 0xaa, 0xaa}},
    {{0x49, 0x5a, 0x96, 0xba, 0x6b, 0xad, 0x78, 0x24, 0x07, 0xbd, 0x52,
      0x1a, 0x00, 0xba, 0xce, 0x65, 0x7b, 0xb3, 0x55, 0x55, 0x5e, 0x4b,
      0xb7, 0xf8, 0x14, 0x6c, 0x71, 0xbb, 0xa5, 0x7e, 0x7a, 0xce}},
    {{0x4b, 0xa6, 0x03, 0x1c, 0xa3, 0x05, 0xb0, 0x9e, 0x53, 0xbd, 0xe3,
      0x70, 0x51, 0x45, 0x48, 0x1d, 0x03, 0x32, 0xb6, 0x51, 0xfe, 0x30,
      0x37, 0x0d, 0xd5, 0x25, 0x4c, 0xc4, 0xd2, 0xcb, 0x32, 0xf3}},
    {{0x51, 0x92, 0x43, 0x8e, 0xc3, 0x69, 0xd7, 0xee, 0x0c, 0xe7, 0x1f,
      0x5c, 0x6d, 0xb7, 0x5f, 0x94, 0x1e, 0xfb, 0xf7, 0x2e, 0x58, 0x44,
      0x17, 0x15, 0xe9, 0x9e, 0xab, 0x04, 0xc2, 0xc8, 0xac, 0xee}},
    {{0x56, 0x7b, 0x82, 0x11, 0xfd, 0x20, 0xd3, 0xd2, 0x83, 0xee, 0x0c,
      0xd7, 0xce, 0x06, 0x72, 0xcb, 0x9d, 0x99, 0xbc, 0x5b, 0x48, 0x7a,
      0x58, 0xc9, 0xd5, 0x4e, 0xc6, 0x7f, 0x77, 0xd4, 0xa8, 0xf5}},
    {{0x5c, 0x4f, 0x28, 0x53, 0x88, 0xf3, 0x83, 0x36, 0x26, 0x9a, 0x55,
      0xc7, 0xc1, 0x2c, 0x0b, 0x3c, 0xa7, 0x3f, 0xef, 0x2a, 0x5a, 0x4d,
      0xf8, 0x2b, 0x89, 0x14, 0x1e, 0x84, 0x1a, 0x6c, 0x4d, 0xe4}},
    {{0x67, 0xdc, 0x4f, 0x32, 0xfa, 0x10, 0xe7, 0xd0, 0x1a, 0x79, 0xa0,
      0x73, 0xaa, 0x0c, 0x9e, 0x02, 0x12, 0xec, 0x2f, 0xfc, 0x3d, 0x77,
      0x9e, 0x0a, 0xa7, 0xf9, 0xc0, 0xf0, 0xe1, 0xc2, 0xc8, 0x93}},
    {{0x6b, 0x86, 0xde, 0x96, 0xa6, 0x58, 0xa5, 0x68, 0x20, 0xa4, 0xf3,
      0x5d, 0x90, 0xdb, 0x6c, 0x3e, 0xfd, 0xd5, 0x74, 0xce, 0x94, 0xb9,
      0x09, 0xcb, 0x0d, 0x7f, 0xf1, 0x7c, 0x3c, 0x18, 0x9d, 0x83}},
    {{0x70, 0x06, 0xa3, 0x83, 0x11, 0xe5, 0x8f, 0xb1, 0x93, 0x48, 0x42,
      0x33, 0x21, 0x82, 0x10, 0xc6, 0x61, 0x25, 0xa0, 0xe4, 0xa8, 0x26,
      0xae, 0xd5, 0x39, 0xac, 0x56, 0x1d, 0xfb, 0xfb, 0xd9, 0x03}},
    {{0x78, 0x1f, 0x1c, 0x3a, 0x6a, 0x42, 0xe3, 0xe9, 0x15, 0x22, 0x2d,
      0xb4, 0x96, 0x77, 0x02, 0xa2, 0xe5, 0x77, 0xae, 0xb0, 0x17, 0x07,
      0x5f, 0xa3, 0xc1, 0x59, 0x85, 0x1f, 0xdd, 0xd0, 0x53, 0x5e}},
    {{0x7c, 0xaa, 0x03, 0x46, 0x51, 0x24, 0x59, 0x0c, 0x60, 0x1e, 0x56,
      0x7e, 0x52, 0x14, 0x8e, 0x95, 0x2c, 0x0c, 0xff, 0xe8, 0x90, 0x00,
      0x53, 0x0f, 0xe0, 0xd9, 0x5b, 0x6d, 0x50, 0xea, 0xae, 0x41}},
    {{0x80, 0x9f, 0x2b, 0xaa, 0xe3, 0x5a, 0xfb, 0x4f, 0x36, 0xbd, 0x64,
      0x76, 0xce, 0x75, 0xc2, 0x00, 0x10, 0x77, 0x90, 0x1b, 0x6a, 0xf5,
      0xc4, 0xda, 0xb8, 0x2e, 0x18, 0x8c, 0x6b, 0x95, 0xc1, 0xa1}},
    {{0x81, 0xa9, 0x8f, 0xc7, 0x88, 0xc3, 0x5f, 0x55, 0x76, 0x45, 0xa9,
      0x52, 0x24, 0xe5, 0x0c, 0xd1, 0xda, 0xc8, 0xff, 0xb2, 0x09, 0xdc,
      0x1e, 0x56, 0x88, 0xaa, 0x29, 0x20, 0x5f, 0x13, 0x22, 0x18}},
    {{0x86, 0x0a, 0x7f, 0x19, 0x21, 0x0d, 0x5e, 0xad, 0x05, 0x7a, 0x78,
      0x53, 0x2b, 0x80, 0x95, 0x14, 0x53, 0xcb, 0x29, 0x07, 0x31, 0x5f,
      0x3b, 0xa7, 0xaa, 0x47, 0xb6, 0x98, 0x97, 0xd7, 0x0f, 0x3f}},
    {{0x87, 0xaf, 0x34, 0xd6, 0x6f, 0xb3, 0xf2, 0xfd, 0xf3, 0x6e, 0x09,
      0x11, 0x1e, 0x9a, 0xba, 0x2f, 0x6f, 0x44, 0xb2, 0x07, 0xf3, 0x86,
      0x3f, 0x3d, 0x0b, 0x54, 0xb2, 0x50, 0x23, 0x90, 0x9a, 0xa5}},
    {{0x95, 0x73, 0x54, 0x73, 0xbd, 0x67, 0xa3, 0xb9, 0x5a, 0x8d, 0x5f,
      0x90, 0xc5, 0xa2, 0x1a, 0xce, 0x1e, 0x0d, 0x79, 0x47, 0x32, 0x06,
      0x74, 0xd4, 0xab, 0x84, 0x79, 0x72, 0xb9, 0x15, 0x44, 0xd2}},
    {{0x96, 0x7b, 0x0c, 0xd9, 0x3f, 0xce, 0xf7, 0xf2, 0x7c, 0xe2, 0xc2,
      0x45, 0x76, 0x7a, 0xe9, 0xb0, 0x5a, 0x77, 0x6b, 0x06, 0x49, 0xf9,
      0x96, 0x5b, 0x62, 0x90, 0x96, 0x84, 0x69, 0x68, 0x68, 0x72}},
    {{0x96, 0x99, 0x22, 0x5c, 0x5d, 0xe5, 0x2e, 0x56, 0xcd, 0xd3, 0x2d,
      0xf2, 0xe9, 0x6d, 0x1c, 0xfe, 0xa5, 0xaa, 0x3c, 0xa0, 0xbb, 0x52,
      0xcd, 0x89, 0x33, 0xc2, 0x3b, 0x5c, 0x27, 0x44, 0x38, 0x20}},
    {{0x9c, 0x6f, 0x6a, 0x12, 0x3c, 0xba, 0xa4, 0xee, 0x34, 0xdb, 0xec,
      0xee, 0xe2, 0x4c, 0x97, 0xd7, 0x38, 0x87, 0x8c, 0xb4, 0x23, 0xf3,
      0xc2, 0x27, 0x39, 0x03, 0x42, 0x4f, 0x5d, 0x1f, 0x6d, 0xd5}},
    {{0xa6, 0xf1, 0xf9, 0xbf, 0x8a, 0x0a, 0x9d, 0xdc, 0x08, 0x0f, 0xb4,
      0x9b, 0x1e, 0xfc, 0x3d, 0x1a, 0x1c, 0x2c, 0x32, 0xdc, 0x0e, 0x13,
      0x6a, 0x5b, 0x00, 0xc9, 0x73, 0x16, 0xf2, 0xa3, 0xdc, 0x11}},
    {{0xab, 0x38, 0x76, 0xc3, 0xda, 0x5d, 0xe0, 0xc9, 0xcf, 0x67, 0x36,
      0x86, 0x8e, 0xe5, 0xb8, 0x8b, 0xf9, 0xba, 0x1d, 0xff, 0x9c, 0x9d,
      0x72, 0xd2, 0xfe, 0x5a, 0x8d, 0x2f, 0x78, 0x30, 0x21, 0x66}},
    {{0xab, 0x39, 0xa4, 0xb0, 0x25, 0x95, 0x56, 0x91, 0xa4, 0x02, 0x69,
      0xf3, 0x53, 0xfa, 0x1d, 0x5c, 0xb9, 0x4e, 0xaf, 0x6c, 0x7e, 0xa9,
      0x80, 0x84, 0x84, 0xbb, 0xbb, 0x62, 0xfd, 0x9f, 0x68, 0xf3}},
    {{0xab, 0x5c, 0xdb, 0x33, 0x56, 0x39, 0x73, 0x56, 0xd6, 0xe6, 0x91,
      0x97, 0x3c, 0x25, 0xb8, 0x61, 0x8b, 0x65, 0xd7, 0x6a, 0x90, 0x48,
      0x6e, 0xa7, 0xa8, 0xa5, 0xc1, 0x77, 0x67, 0xf4, 0x67, 0x3a}},
    {{0xab, 0x98, 0x49, 0x52, 0x76, 0xad, 0xf1, 0xec, 0xaf, 0xf2, 0x8f,
      0x35, 0xc5, 0x30, 0x48, 0x78, 0x1e, 0x5c, 0x17, 0x18, 0xda, 0xb9,
      0xc8, 0xe6, 0x7a, 0x50, 0x4f, 0x4f, 0x6a, 0x51, 0x32, 0x8f}},
    {{0xac, 0xf6, 0x5e, 0x1d, 0x62, 0xcb, 0x58, 0xa2, 0xba, 0xfd, 0x6f,
      0xfa, 0xb4, 0x0f, 0xb8, 0x86, 0x99, 0xc4, 0x73, 0x97, 0xcf, 0x5c,
      0xb4, 0x83, 0xd4, 0x2d, 0x69, 0xca, 0xd3, 0x4c, 0xd4, 0x8b}},
    {{0xaf, 0x20, 0x7c, 0x61, 0xfd, 0x9c, 0x7c, 0xf9, 0x2c, 0x2a, 0xfe,
      0x81, 0x54, 0x28, 0x2d, 0xc3, 0xf2, 0xcb, 0xf3, 0x2f, 0x75, 0xcd,
      0x17, 0x28, 0x14, 0xc5, 0x2b, 0x03, 0xb7, 0xeb, 0xc2, 0x58}},
    {{0xb1, 0x12, 0x41, 0x42, 0xa5, 0xa1, 0xa5, 0xa2, 0x88, 0x19, 0xc7,
      0x35, 0x34, 0x0e, 0xff, 0x8c, 0x9e, 0x2f, 0x81, 0x68, 0xfe, 0xe3,
      0xba, 0x18, 0x7f, 0x25, 0x3b, 0xc1, 0xa3, 0x92, 0xd7, 0xe2}},
    {{0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
      0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
      0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}},
    {{0xbc, 0xfb, 0x44, 0xaa, 0xb9, 0xad, 0x02, 0x10, 0x15, 0x70, 0x6b,
      0x41, 0x21, 0xea, 0x76, 0x1c, 0x81, 0xc9, 0xe8, 0x89, 0x67, 0x59,
      0x0f, 0x6f, 0x94, 0xae, 0x74, 0x4d, 0xc8, 0x8b, 0x78, 0xfb}},
    {{0xc0, 0x71, 0x35, 0xf6, 0xb4, 0x52, 0x39, 0x82, 0x64, 0xa4, 0x77,
      0x6d, 0xbd, 0x0a, 0x6a, 0x30, 0x7c, 0x60, 0xa3, 0x6f, 0x96, 0x7b,
      0xd2, 0x63, 0x21, 0xdc, 0xb8, 0x17, 0xb5, 0xc0, 0xc4, 0x81}},
    {{0xca, 0xb4, 0x82, 0xcd, 0x3e, 0x82, 0x0c, 0x5c, 0xe7, 0x2a, 0xa3,
      0xb6, 0xfd, 0xbe, 0x98, 0x8b, 0xb8, 0xa4, 0xf0, 0x40, 0x7e, 0xca,
      0xfd, 0x8c, 0x92, 0x6e, 0x36, 0x82, 0x4e, 0xab, 0x92, 0xdd}},
    {{0xd2, 0xf9, 0x1a, 0x04, 0xe3, 0xa6, 0x1d, 0x4e, 0xad, 0x78, 0x48,
      0xc8, 0xd4, 0x3b, 0x5e, 0x11, 0x52, 0xd8, 0x85, 0x72, 0x74, 0x89,
      0xbc, 0x65, 0x73, 0x8b, 0x67, 0xc0, 0xa2, 0x27, 0x85, 0xa7}},
    {{0xd3, 0xa2, 0x5d, 0xa8, 0x0d, 0xb7, 0xba, 0xb1, 0x29, 0xa0, 0x66,
      0xab, 0x41, 0x50, 0x3d, 0xdd, 0xff, 0xa0, 0x2c, 0x76, 0x8c, 0x05,
      0x89, 0xf9, 0x9f, 0xd7, 0x11, 0x93, 0xe6, 0x99, 0x16, 0xb6}},
    {{0xd4, 0xaf, 0x6c, 0x0a, 0x48, 0x23, 0x10, 0xbd, 0x7c, 0x54, 0xbb,
      0x7a, 0xb1, 0x21, 0x91, 0x6f, 0x86, 0xc0, 0xc0, 0x7c, 0xd5, 0x2f,
      0xca, 0xc3, 0x2d, 0x38, 0x44, 0xc2, 0x60, 0x05, 0x11, 0x5f}},
    {{0xda, 0x80, 0x0b, 0x80, 0xb2, 0xa8, 0x7d, 0x39, 0x9e, 0x66, 0xfa,
      0x19, 0xd7, 0x2f, 0xdf, 0x49, 0x98, 0x3b, 0x47, 0xd8, 0xcf, 0x32,
      0x2c, 0x7c, 0x79, 0x50, 0x3a, 0x0c, 0x7e, 0x28, 0xfe, 0xaf}},
    {{0xf1, 0x5f, 0x1d, 0x32, 0x3e, 0xd9, 0xca, 0x98, 0xe9, 0xea, 0x95,
      0xb3, 0x3e, 0xc5, 0xdd, 0xa4, 0x7e, 0xa4, 0xc3, 0x29, 0xf9, 0x52,
      0xc1, 0x6f, 0x65, 0xad, 0x41, 0x9e, 0x64, 0x52, 0x04, 0x76}},
    {{0xf2, 0xe9, 0x36, 0x5e, 0xa1, 0x21, 0xdf, 0x5e, 0xeb, 0xd8, 0xde,
      0x24, 0x68, 0xfd, 0xc1, 0x71, 0xdc, 0x0a, 0x9e, 0x46, 0xda, 0xdc,
      0x1a, 0xb4, 0x1d, 0x52, 0x79, 0x0b, 0xa9, 0x80, 0xa7, 0xc2}},
    {{0xf5, 0x3c, 0x22, 0x05, 0x98, 0x17, 0xdd, 0x96, 0xf4, 0x00, 0x65,
      0x16, 0x39, 0xd2, 0xf8, 0x57, 0xe2, 0x10, 0x70, 0xa5, 0x9a, 0xbe,
      0xd9, 0x07, 0x94, 0x00, 0xd9, 0xf6, 0x95, 0x50, 0x69, 0x00}},
    {{0xf6, 0xb5, 0x9c, 0x8e, 0x27, 0x89, 0xa1, 0xfd, 0x5d, 0x5b, 0x25,
      0x37, 0x42, 0xfe, 0xad, 0xc6, 0x92, 0x5c, 0xb9, 0x3e, 0xdc, 0x34,
      0x5e, 0x53, 0x16, 0x6e, 0x12, 0xc5, 0x2b, 0xa2, 0xa6, 0x01}},
    {{0xff, 0x56, 0x80, 0xcd, 0x73, 0xa5, 0x70, 0x3d, 0xa0, 0x48, 0x17,
      0xa0, 0x75, 0xfd, 0x46, 0x25, 0x06, 0xa7, 0x35, 0x06, 0xc4, 0xb8,
      0x1a, 0x15, 0x83, 0xef, 0x54, 0x94, 0x78, 0xd2, 0x64, 0x76}},
};

const SHA256HashValue kSymantecExceptions[] = {
    {{0x0b, 0x2a, 0xc7, 0xee, 0x53, 0xe1, 0xb3, 0xb2, 0x12, 0xcf, 0xd1,
      0x95, 0xfb, 0xe4, 0x29, 0xc3, 0x7b, 0x0a, 0xc4, 0xb1, 0x6e, 0xa0,
      0xb8, 0xf6, 0xcf, 0xe4, 0xfe, 0xd9, 0x9b, 0x2e, 0x19, 0xb6}},
    {{0x56, 0xe9, 0x8d, 0xea, 0xc0, 0x06, 0xa7, 0x29, 0xaf, 0xa2, 0xed,
      0x79, 0xf9, 0xe4, 0x19, 0xdf, 0x69, 0xf4, 0x51, 0x24, 0x25, 0x96,
      0xd2, 0xaa, 0xf2, 0x84, 0xc7, 0x4a, 0x85, 0x5e, 0x35, 0x2e}},
    {{0x72, 0x89, 0xc0, 0x6d, 0xed, 0xd1, 0x6b, 0x71, 0xa7, 0xdc, 0xca,
      0x66, 0x57, 0x85, 0x72, 0xe2, 0xe1, 0x09, 0xb1, 0x1d, 0x70, 0xad,
      0x04, 0xc2, 0x60, 0x1b, 0x67, 0x43, 0xbc, 0x66, 0xd0, 0x7b}},
    {{0xa5, 0x3d, 0xb6, 0x10, 0x6b, 0xb7, 0x60, 0x35, 0x4c, 0xed, 0x90,
      0x45, 0xfb, 0x06, 0xeb, 0x06, 0xaf, 0x85, 0xe6, 0xb1, 0x18, 0xc0,
      0x62, 0xed, 0x13, 0xc3, 0x05, 0x1d, 0xfe, 0xb8, 0xd4, 0x9c}},
    {{0xb5, 0xcf, 0x82, 0xd4, 0x7e, 0xf9, 0x82, 0x3f, 0x9a, 0xa7, 0x8f,
      0x12, 0x31, 0x86, 0xc5, 0x2e, 0x88, 0x79, 0xea, 0x84, 0xb0, 0xf8,
      0x22, 0xc9, 0x1d, 0x83, 0xe0, 0x42, 0x79, 0xb7, 0x8f, 0xd5}},
    {{0xc0, 0x55, 0x4b, 0xde, 0x87, 0xa0, 0x75, 0xec, 0x13, 0xa6, 0x1f,
      0x27, 0x59, 0x83, 0xae, 0x02, 0x39, 0x57, 0x29, 0x4b, 0x45, 0x4c,
      0xaf, 0x0a, 0x97, 0x24, 0xe3, 0xb2, 0x1b, 0x79, 0x35, 0xbc}},
    {{0xe2, 0x4f, 0x8e, 0x8c, 0x21, 0x85, 0xda, 0x2f, 0x5e, 0x88, 0xd4,
      0x57, 0x9e, 0x81, 0x7c, 0x47, 0xbf, 0x6e, 0xaf, 0xbc, 0x85, 0x05,
      0xf0, 0xf9, 0x60, 0xfd, 0x5a, 0x0d, 0xf4, 0x47, 0x3a, 0xd3}},
    {{0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
      0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
      0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}},
    {{0xfa, 0xe4, 0x60, 0x00, 0xd8, 0xf7, 0x04, 0x25, 0x58, 0x54, 0x1e,
      0x98, 0xac, 0xf3, 0x51, 0x27, 0x95, 0x89, 0xf8, 0x3b, 0x6d, 0x30,
      0x01, 0xc1, 0x84, 0x42, 0xe4, 0x40, 0x3d, 0x11, 0x18, 0x49}},
};

const CTRequiredPolicy kCTRequiredPolicies[] = {
    {
        kSymantecRoots, arraysize(kSymantecRoots),
        // 1 June 2016, 00:00:00 GMT.
        base::TimeDelta::FromSeconds(1464739200), kSymantecExceptions,
        arraysize(kSymantecExceptions),
    },
};
