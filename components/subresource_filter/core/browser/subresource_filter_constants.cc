// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

namespace subresource_filter {

const base::FilePath::CharType kTopLevelDirectoryName[] =
    FILE_PATH_LITERAL("Subresource Filter");

const base::FilePath::CharType kIndexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Indexed Rules");

const base::FilePath::CharType kUnindexedRulesetBaseDirectoryName[] =
    FILE_PATH_LITERAL("Unindexed Rules");

const base::FilePath::CharType kRulesetDataFileName[] =
    FILE_PATH_LITERAL("Ruleset Data");

const base::FilePath::CharType kLicenseFileName[] =
    FILE_PATH_LITERAL("LICENSE");

}  // namespace subresource_filter
