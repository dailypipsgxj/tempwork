// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/string16.h"
#include "net/base/filename_util_internal.h"

class GURL;

namespace net {

bool IsSafePortablePathComponent(const base::FilePath& component) {
  base::string16 component16;
  base::FilePath::StringType sanitized = component.value();
  SanitizeGeneratedFileName(&sanitized, true);
  base::FilePath::StringType extension = component.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.
  return !component.empty() && (component == component.BaseName()) &&
         (component == component.StripTrailingSeparators()) &&
         FilePathToString16(component, &component16) &&
         base::i18n::IsFilenameLegal(component16) &&
         !IsShellIntegratedExtension(extension) &&
         (sanitized == component.value()) &&
         !IsReservedNameOnWindows(component.value());
}

bool IsSafePortableRelativePath(const base::FilePath& path) {
  if (path.empty() || path.IsAbsolute() || path.EndsWithSeparator())
    return false;
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.empty())
    return false;
  for (size_t i = 0; i < components.size() - 1; ++i) {
    if (!IsSafePortablePathComponent(base::FilePath(components[i])))
      return false;
  }
  return IsSafePortablePathComponent(path.BaseName());
}

base::string16 GetSuggestedFilename(const GURL& url,
                                    const std::string& content_disposition,
                                    const std::string& referrer_charset,
                                    const std::string& suggested_name,
                                    const std::string& mime_type,
                                    const std::string& default_name) {
  return GetSuggestedFilenameImpl(
      url,
      content_disposition,
      referrer_charset,
      suggested_name,
      mime_type,
      default_name,
      base::Bind(&base::i18n::ReplaceIllegalCharactersInPath));
}

base::FilePath GenerateFileName(const GURL& url,
                                const std::string& content_disposition,
                                const std::string& referrer_charset,
                                const std::string& suggested_name,
                                const std::string& mime_type,
                                const std::string& default_file_name) {
  base::FilePath generated_name(GenerateFileNameImpl(
      url,
      content_disposition,
      referrer_charset,
      suggested_name,
      mime_type,
      default_file_name,
      base::Bind(&base::i18n::ReplaceIllegalCharactersInPath)));

#if defined(OS_CHROMEOS)
  // When doing file manager operations on ChromeOS, the file paths get
  // normalized in WebKit layer, so let's ensure downloaded files have
  // normalized names. Otherwise, we won't be able to handle files with NFD
  // utf8 encoded characters in name.
  base::i18n::NormalizeFileNameEncoding(&generated_name);
#endif

  DCHECK(!generated_name.empty());

  return generated_name;
}

}  // namespace net
