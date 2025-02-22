// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MIME_UTIL_H__
#define NET_BASE_MIME_UTIL_H__

// This file defines MIME utility functions. All of them assume the MIME type
// to be of the format specified by rfc2045. According to it, MIME types are
// case strongly insensitive except parameter values, which may or may not be
// case sensitive.
//
// These utilities perform a *case-sensitive* matching for  parameter values,
// which may produce some false negatives. Except that, matching is
// case-insensitive.
//
// All constants in mime_util.cc must be written in lower case, except parameter
// values, which can be any case.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "net/base/net_export.h"

namespace net {

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
NET_EXPORT bool GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                         std::string* mime_type);

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists. In this method,
// the search for a mime type is constrained to a limited set of
// types known to the net library, the OS/registry is not consulted.
NET_EXPORT bool GetWellKnownMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  Returns
// true if a corresponding mime type exists.
NET_EXPORT bool GetMimeTypeFromFile(const base::FilePath& file_path,
                                    std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.  The extension is
// returned without a prefixed dot, ex "html".
NET_EXPORT bool GetPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* extension);

// Returns true if this the mime_type_pattern matches a given mime-type.
// Checks for absolute matching and wildcards. MIME types are case insensitive.
NET_EXPORT bool MatchesMimeType(const std::string& mime_type_pattern,
                                const std::string& mime_type);

// Returns true if the |type_string| is a correctly-formed mime type specifier
// with no parameter, i.e. string that matches the following ABNF (see the
// definition of content ABNF in RFC2045 and media-type ABNF httpbis p2
// semantics).
//
//   token "/" token
//
// If |top_level_type| is non-NULL, sets it to parsed top-level type string.
// If |subtype| is non-NULL, sets it to parsed subtype string.
NET_EXPORT bool ParseMimeTypeWithoutParameter(const std::string& type_string,
                                              std::string* top_level_type,
                                              std::string* subtype);

// Returns true if the |type_string| is a top-level type of any media type
// registered with IANA media types registry at
// http://www.iana.org/assignments/media-types/media-types.xhtml or an
// experimental type (type with x- prefix).
//
// This method doesn't check that the input conforms to token ABNF, so if input
// is experimental type strings, you need to check check that before using
// this method.
NET_EXPORT bool IsValidTopLevelMimeType(const std::string& type_string);

// Get the extensions associated with the given mime type. There could be
// multiple extensions for a given mime type, like "html,htm" for "text/html",
// or "txt,text,html,..." for "text/*".
// Note that we do not erase the existing elements in the the provided vector.
// Instead, we append the result to it.
NET_EXPORT void GetExtensionsForMimeType(
    const std::string& mime_type,
    std::vector<base::FilePath::StringType>* extensions);

// A list of supported certificate-related mime types.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum CertificateMimeType {
  CERTIFICATE_MIME_TYPE_UNKNOWN,
  CERTIFICATE_MIME_TYPE_X509_USER_CERT,
  CERTIFICATE_MIME_TYPE_X509_CA_CERT,
  CERTIFICATE_MIME_TYPE_PKCS12_ARCHIVE,
};

// Generates a random MIME multipart boundary.
// The returned string is guaranteed to be at most 70 characters long.
NET_EXPORT std::string GenerateMimeMultipartBoundary();

// Prepares one value as part of a multi-part upload request.
NET_EXPORT void AddMultipartValueForUpload(const std::string& value_name,
                                           const std::string& value,
                                           const std::string& mime_boundary,
                                           const std::string& content_type,
                                           std::string* post_data);

// Adds the final delimiter to a multi-part upload request.
NET_EXPORT void AddMultipartFinalDelimiterForUpload(
    const std::string& mime_boundary,
    std::string* post_data);

struct MimeInfo {
  const char* const mime_type;
  const char* const extensions;  // comma separated list
};

// Finds mime type of |ext| from |mappings|.
const char* FindMimeType(const MimeInfo* mappings,
                         size_t mappings_len,
                         const std::string& ext);

}  // namespace net

#endif  // NET_BASE_MIME_UTIL_H__
