// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/string16.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"

namespace base {
class FeatureList;
class ListValue;
}

namespace flags_ui {
class FlagsStorage;
}

namespace about_flags {

// Reads the state from |flags_storage| and adds the command line flags
// belonging to the active feature entries to |command_line|.
void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels);

// Registers variations parameter values selected for features in about:flags.
// The selected flags are retrieved from |flags_storage|, the registered
// variation parameters are connected to their corresponding features in
// |feature_list|. Returns the (possibly empty) list of additional variation ids
// to register in the MetricsService that come from variations selected using
// chrome://flags.
std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list);

// Compares a set of switches of the two provided command line objects and
// returns true if they are the same and false otherwise.
// If |out_difference| is not NULL, it's filled with set_symmetric_difference
// between sets.
bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference);

// Gets the list of feature entries. Entries that are available for the current
// platform are appended to |supported_entries|; all other entries are appended
// to |unsupported_entries|.
void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries);

// Returns true if one of the feature entry flags has been flipped since
// startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the current with id |internal_name|.
void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);

// Removes all switches that were added to a command line by a previous call to
// |ConvertFlagsToSwitches()|.
void RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);

// Sends UMA stats about experimental flag usage. This should be called once per
// startup.
void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage);

// Returns the UMA id for the specified switch name.
base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name);

// Sends stats (as UMA histogram) about command_line_difference.
// This is used on ChromeOS to report flags that lead to browser restart.
// |command_line_difference| is the result of
// AreSwitchesIdenticalToCurrentCommandLine().
void ReportCustomFlags(const std::string& uma_histogram_hame,
                       const std::set<std::string>& command_line_difference);

namespace testing {

// Returns the global set of feature entries.
const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count);

// This value is reported as switch histogram ID if switch name has unknown
// format.
extern const base::HistogramBase::Sample kBadSwitchFormatHistogramId;

}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
