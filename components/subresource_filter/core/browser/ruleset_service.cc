// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_proxy.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"

namespace subresource_filter {

// Constant definitions and helper functions ---------------------------------

namespace {

// Names of the preferences storing the most recent ruleset version that
// was successfully stored to disk.
const char kSubresourceFilterRulesetContentVersion[] =
    "subresource_filter.ruleset_version.content";
const char kSubresourceFilterRulesetFormatVersion[] =
    "subresource_filter.ruleset_version.format";

}  // namespace

// UnindexedRulesetInfo ------------------------------------------------------

UnindexedRulesetInfo::UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::~UnindexedRulesetInfo() = default;

// IndexedRulesetVersion ------------------------------------------------------

IndexedRulesetVersion::IndexedRulesetVersion() = default;
IndexedRulesetVersion::IndexedRulesetVersion(const std::string& content_version,
                                             int format_version)
    : content_version(content_version), format_version(format_version) {}
IndexedRulesetVersion::~IndexedRulesetVersion() = default;
IndexedRulesetVersion& IndexedRulesetVersion::operator=(
    const IndexedRulesetVersion&) = default;

// static
void IndexedRulesetVersion::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSubresourceFilterRulesetContentVersion,
                               std::string());
  registry->RegisterIntegerPref(kSubresourceFilterRulesetFormatVersion, 0);
}

// static
int IndexedRulesetVersion::CurrentFormatVersion() {
  return RulesetIndexer::kIndexedFormatVersion;
}

void IndexedRulesetVersion::ReadFromPrefs(PrefService* local_state) {
  format_version =
      local_state->GetInteger(kSubresourceFilterRulesetFormatVersion);
  content_version =
      local_state->GetString(kSubresourceFilterRulesetContentVersion);
}

bool IndexedRulesetVersion::IsValid() const {
  return format_version != 0 && !content_version.empty();
}

bool IndexedRulesetVersion::IsCurrentFormatVersion() const {
  return format_version == CurrentFormatVersion();
}

void IndexedRulesetVersion::SaveToPrefs(PrefService* local_state) const {
  local_state->SetInteger(kSubresourceFilterRulesetFormatVersion,
                          format_version);
  local_state->SetString(kSubresourceFilterRulesetContentVersion,
                         content_version);
}

base::FilePath IndexedRulesetVersion::GetSubdirectoryPathForVersion(
    const base::FilePath& base_dir) const {
  return base_dir.AppendASCII(base::IntToString(format_version))
      .AppendASCII(content_version);
}

// RulesetService ------------------------------------------------------------

RulesetService::RulesetService(
    PrefService* local_state,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& indexed_ruleset_base_dir)
    : local_state_(local_state),
      blocking_task_runner_(blocking_task_runner),
      indexed_ruleset_base_dir_(indexed_ruleset_base_dir) {
  DCHECK_NE(local_state_->GetInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_WAITING);
  IndexedRulesetVersion most_recently_indexed_version;
  most_recently_indexed_version.ReadFromPrefs(local_state_);
  if (most_recently_indexed_version.IsValid() &&
      most_recently_indexed_version.IsCurrentFormatVersion())
    OpenAndPublishRuleset(most_recently_indexed_version);
}

RulesetService::~RulesetService() {}

void RulesetService::IndexAndStoreAndPublishRulesetIfNeeded(
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  // Trying to store a ruleset with the same version for a second time would not
  // only be futile, but would fail on Windows due to "File System Tunneling" as
  // long as the previously stored copy of the rules is still in use.
  IndexedRulesetVersion most_recently_indexed_version;
  most_recently_indexed_version.ReadFromPrefs(local_state_);
  if (most_recently_indexed_version.IsCurrentFormatVersion() &&
      most_recently_indexed_version.content_version ==
          unindexed_ruleset_info.content_version)
    return;

  IndexAndStoreRuleset(
      unindexed_ruleset_info,
      base::Bind(&RulesetService::OpenAndPublishRuleset, AsWeakPtr()));
}

void RulesetService::RegisterDistributor(
    std::unique_ptr<RulesetDistributor> distributor) {
  if (ruleset_data_ && ruleset_data_->IsValid())
    distributor->PublishNewVersion(ruleset_data_->DuplicateFile());
  distributors_.push_back(std::move(distributor));
}

// static
base::FilePath RulesetService::GetRulesetDataFilePath(
    const base::FilePath& version_directory) {
  return version_directory.Append(kRulesetDataFileName);
}

// static
base::FilePath RulesetService::GetLicenseFilePath(
    const base::FilePath& version_directory) {
  return version_directory.Append(kLicenseFileName);
}

// static
IndexedRulesetVersion RulesetService::IndexAndWriteRuleset(
    const base::FilePath& indexed_ruleset_base_dir,
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::string unindexed_ruleset_data;
  if (!base::ReadFileToString(unindexed_ruleset_info.ruleset_path,
                              &unindexed_ruleset_data)) {
    return IndexedRulesetVersion();
  }

  // TODO(pkalinnikov): Implement streaming wire format here.
  proto::UrlRule rule;
  if (!rule.ParseFromString(unindexed_ruleset_data))
    return IndexedRulesetVersion();

  RulesetIndexer indexer;
  indexer.AddUrlRule(rule);
  indexer.Finish();

  IndexedRulesetVersion indexed_version(
      unindexed_ruleset_info.content_version,
      IndexedRulesetVersion::CurrentFormatVersion());
  WriteRulesetResult result = WriteRuleset(
      indexed_ruleset_base_dir, indexed_version,
      unindexed_ruleset_info.license_path, indexer.data(), indexer.size());
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.WriteRuleset.Result",
                            static_cast<int>(result),
                            static_cast<int>(WriteRulesetResult::MAX));

  if (result != WriteRulesetResult::SUCCESS)
    return IndexedRulesetVersion();

  DCHECK(indexed_version.IsValid());
  return indexed_version;
}

// static
RulesetService::WriteRulesetResult RulesetService::WriteRuleset(
    const base::FilePath& indexed_ruleset_base_dir,
    const IndexedRulesetVersion& indexed_version,
    const base::FilePath& license_path,
    const uint8_t* indexed_ruleset_data,
    size_t indexed_ruleset_size) {
  base::FilePath indexed_ruleset_version_dir =
      indexed_version.GetSubdirectoryPathForVersion(indexed_ruleset_base_dir);
  base::ScopedTempDir scratch_dir;
  if (!scratch_dir.CreateUniqueTempDirUnderPath(
          indexed_ruleset_version_dir.DirName())) {
    return WriteRulesetResult::FAILED_CREATING_SCRATCH_DIR;
  }

  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  const int data_size_in_chars = base::checked_cast<int>(indexed_ruleset_size);
  if (base::WriteFile(GetRulesetDataFilePath(scratch_dir.path()),
                      reinterpret_cast<const char*>(indexed_ruleset_data),
                      data_size_in_chars) != data_size_in_chars) {
    return WriteRulesetResult::FAILED_WRITING_RULESET_DATA;
  }

  if (base::PathExists(license_path) &&
      !base::CopyFile(license_path, GetLicenseFilePath(scratch_dir.path()))) {
    return WriteRulesetResult::FAILED_WRITING_LICENSE;
  }

  // Creating a temporary directory also makes sure the path (except for the
  // final segment) gets created. ReplaceFile would not create the path.
  DCHECK(base::PathExists(indexed_ruleset_version_dir.DirName()));

  // This will attempt to overwrite the previously stored ruleset with the same
  // version, if any. Doing so is needed in case the earlier write was
  // interrupted, but will fail on Windows in case the earlier write was
  // successful and the ruleset is in use. We should not normally get to here in
  // that case, however, due to the same-version check above. Even if we do, the
  // worst-case scenario is that a slightly-older ruleset version will be used
  // until next restart/ruleset update.
  base::File::Error error;
  if (!base::ReplaceFile(scratch_dir.path(), indexed_ruleset_version_dir,
                         &error)) {
    // While enumerators of base::File::Error all have negative values, the
    // histogram records the absolute values.
    UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.WriteRuleset.ReplaceFileError",
                              -error, -base::File::FILE_ERROR_MAX);
    return WriteRulesetResult::FAILED_REPLACE_FILE;
  }

  scratch_dir.Take();
  return WriteRulesetResult::SUCCESS;
}

void RulesetService::IndexAndStoreRuleset(
    const UnindexedRulesetInfo& unindexed_ruleset_info,
    const WriteRulesetCallback& success_callback) {
  if (unindexed_ruleset_info.content_version.empty())
    return;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&RulesetService::IndexAndWriteRuleset,
                 indexed_ruleset_base_dir_, unindexed_ruleset_info),
      base::Bind(&RulesetService::OnWrittenRuleset, AsWeakPtr(),
                 success_callback));
}

void RulesetService::OnWrittenRuleset(
    const WriteRulesetCallback& result_callback,
    const IndexedRulesetVersion& version) {
  DCHECK(!result_callback.is_null());
  if (!version.IsValid())
    return;
  version.SaveToPrefs(local_state_);
  result_callback.Run(version);
}

void RulesetService::OpenAndPublishRuleset(
    const IndexedRulesetVersion& version) {
  ruleset_data_.reset(new base::FileProxy(blocking_task_runner_.get()));
  // On Windows, open the file with FLAG_SHARE_DELETE to allow deletion while
  // there are handles to it still open. Note that creating a new file at the
  // same path would still be impossible until after the last handle is closed.
  ruleset_data_->CreateOrOpen(
      GetRulesetDataFilePath(
          version.GetSubdirectoryPathForVersion(indexed_ruleset_base_dir_)),
      base::File::FLAG_OPEN | base::File::FLAG_READ |
          base::File::FLAG_SHARE_DELETE,
      base::Bind(&RulesetService::OnOpenedRuleset, AsWeakPtr()));
}

void RulesetService::OnOpenedRuleset(base::File::Error error) {
  // The file has just been successfully written, so a failure here is unlikely
  // unless |indexed_ruleset_base_dir_| has been tampered with or there are disk
  // errors. Still, restore the invariant that a valid version in preferences
  // always points to an existing version of disk by invalidating the prefs.
  if (error != base::File::FILE_OK) {
    IndexedRulesetVersion().SaveToPrefs(local_state_);
    return;
  }

  // A second call to OpenAndPublishRuleset() may have reset |ruleset_data_| to
  // a new instance that is in the process of calling CreateOrOpen() on a newer
  // version. Bail out.
  DCHECK(ruleset_data_);
  if (!ruleset_data_->IsValid())
    return;

  DCHECK_EQ(error, base::File::Error::FILE_OK);
  for (auto& distributor : distributors_)
    distributor->PublishNewVersion(ruleset_data_->DuplicateFile());
}

}  // namespace subresource_filter
