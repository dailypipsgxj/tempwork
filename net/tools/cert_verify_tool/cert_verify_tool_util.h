// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_CERT_VERIFY_TOOL_CERT_VERIFY_TOOL_UTIL_H_
#define NET_TOOLS_CERT_VERIFY_TOOL_CERT_VERIFY_TOOL_UTIL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

// Stores DER certificate bytes and details about where they were read from.
// This allows decoupling the input file reading from the certificate parsing
// while retaining useful error messages.
struct CertInput {
  // DER-encoded certificate data. This is not validated.
  std::string der_cert;

  // The source file the data was read from.
  base::FilePath source_file_path;

  // Human-readable details about the source of the data, for logging purposes.
  // For example, if the |source_file_path| contained multiple certificates,
  // this might indicate which part of the file |der_cert| came from.
  std::string source_details;
};

// Parses |file_path| as a single DER cert or a PEM certificate list.
bool ReadCertificatesFromFile(const base::FilePath& file_path,
                              std::vector<CertInput>* certs);

// Parses |file_path| as a DER cert or PEM chain. If more than one cert is
// present, the first will be used as the target certificate and the rest will
// be used as intermediates.
void ReadChainFromFile(const base::FilePath& file_path,
                       CertInput* target,
                       std::vector<CertInput>* intermediates);

// Writes a file and prints an error message if it failed.
bool WriteToFile(const base::FilePath& file_path, const std::string& data);

// Prints an error about the input |cert|. This will include the file the cert
// was read from, as well as which block in the file if it was a PEM file.
void PrintCertError(const std::string& error, const CertInput& cert);

#endif  // NET_TOOLS_CERT_VERIFY_TOOL_CERT_VERIFY_TOOL_UTIL_H_
