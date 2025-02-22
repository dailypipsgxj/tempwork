// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#if defined(OS_ANDROID)
#include <jni.h>
#endif

#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/MostVisitedSites_jni.h"
#endif

using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_tiles {

namespace {

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramPopularName[] = "popular";
const char kHistogramWhitelistName[] = "whitelist";

const base::Feature kDisplaySuggestionsServiceTiles{
    "DisplaySuggestionsServiceTiles", base::FEATURE_ENABLED_BY_DEFAULT};

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram,
                       int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      num_sites,
      num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kPopularSitesFieldTrialName);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;

  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;

#if defined(OS_ANDROID)
  if (Java_MostVisitedSites_isPopularSitesForceEnabled(
          base::android::AttachCurrentThread())) {
    return true;
  }
#endif

  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Determine whether we need any popular suggestions to fill up a grid of
// |num_tiles| tiles.
bool NeedPopularSites(const PrefService* prefs, int num_tiles) {
  if (num_tiles <= prefs->GetInteger(prefs::kNumPersonalSuggestions))
    return false;

  // TODO(treib): Remove after M55.
  const base::ListValue* source_list =
      prefs->GetList(prefs::kDeprecatedNTPSuggestionsIsPersonal);
  // If there aren't enough previous suggestions to fill the grid, we need
  // popular suggestions.
  if (static_cast<int>(source_list->GetSize()) < num_tiles)
    return true;
  // Otherwise, if any of the previous suggestions is not personal, then also
  // get popular suggestions.
  for (int i = 0; i < num_tiles; ++i) {
    bool is_personal = false;
    if (source_list->GetBoolean(i, &is_personal) && !is_personal)
      return true;
  }
  // The whole grid is already filled with personal suggestions, no point in
  // bothering with popular ones.
  return false;
}

bool AreURLsEquivalent(const GURL& url1, const GURL& url2) {
  return url1.host() == url2.host() && url1.path() == url2.path();
}

std::string GetSourceHistogramName(int source) {
  switch (source) {
    case MostVisitedSites::TOP_SITES:
      return kHistogramClientName;
    case MostVisitedSites::POPULAR:
      return kHistogramPopularName;
    case MostVisitedSites::WHITELIST:
      return kHistogramWhitelistName;
    case MostVisitedSites::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

MostVisitedSites::Suggestion::Suggestion() : source(TOP_SITES) {}

MostVisitedSites::Suggestion::~Suggestion() {}

MostVisitedSites::Suggestion::Suggestion(Suggestion&&) = default;
MostVisitedSites::Suggestion&
MostVisitedSites::Suggestion::operator=(Suggestion&&) = default;

MostVisitedSites::MostVisitedSites(
    PrefService* prefs,
    scoped_refptr<history::TopSites> top_sites,
    SuggestionsService* suggestions,
    PopularSites* popular_sites,
    MostVisitedSitesSupervisor* supervisor)
    : prefs_(prefs),
      top_sites_(top_sites),
      suggestions_service_(suggestions),
      popular_sites_(popular_sites),
      supervisor_(supervisor),
      observer_(nullptr),
      num_sites_(0),
      waiting_for_most_visited_sites_(true),
      waiting_for_popular_sites_(true),
      recorded_uma_(false),
      top_sites_observer_(this),
      mv_source_(SUGGESTIONS_SERVICE),
      weak_ptr_factory_(this) {
  DCHECK(suggestions_service_);
  supervisor_->SetObserver(this);
}

MostVisitedSites::~MostVisitedSites() {
  supervisor_->SetObserver(nullptr);
}

void MostVisitedSites::SetMostVisitedURLsObserver(Observer* observer,
                                                  int num_sites) {
  DCHECK(observer);
  observer_ = observer;
  num_sites_ = num_sites;

  if (popular_sites_ && ShouldShowPopularSites() &&
      NeedPopularSites(prefs_, num_sites_)) {
    popular_sites_->StartFetch(
        false, base::Bind(&MostVisitedSites::OnPopularSitesAvailable,
                          base::Unretained(this)));
  } else {
    waiting_for_popular_sites_ = false;
  }

  if (top_sites_) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites_->SyncWithHistory();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    top_sites_observer_.Add(top_sites_.get());
  }

  suggestions_subscription_ = suggestions_service_->AddCallback(
      base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                 base::Unretained(this)));

  // Immediately build the current suggestions, getting personal suggestions
  // from the SuggestionsService's cache or, if that is empty, from TopSites.
  BuildCurrentSuggestions();
  // Also start a request for fresh suggestions.
  suggestions_service_->FetchSuggestionsData();
}

void MostVisitedSites::AddOrRemoveBlacklistedUrl(const GURL& url,
                                                 bool add_url) {
  if (top_sites_) {
    // Always blacklist in the local TopSites.
    if (add_url)
      top_sites_->AddBlacklistedURL(url);
    else
      top_sites_->RemoveBlacklistedURL(url);
  }

  // Only blacklist in the server-side suggestions service if it's active.
  if (mv_source_ == SUGGESTIONS_SERVICE) {
    if (add_url)
      suggestions_service_->BlacklistURL(url);
    else
      suggestions_service_->UndoBlacklistURL(url);
  }
}

void MostVisitedSites::RecordTileTypeMetrics(
    const std::vector<int>& tile_types,
    const std::vector<int>& sources) {
  int counts_per_type[NUM_TILE_TYPES] = {0};
  for (size_t i = 0; i < tile_types.size(); ++i) {
    int tile_type = tile_types[i];
    ++counts_per_type[tile_type];

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", tile_type, NUM_TILE_TYPES);

    std::string histogram = base::StringPrintf(
        "NewTabPage.TileType.%s",
        GetSourceHistogramName(sources[i]).c_str());
    LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsReal",
                              counts_per_type[ICON_REAL]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsColor",
                              counts_per_type[ICON_COLOR]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsGray",
                              counts_per_type[ICON_DEFAULT]);
}

void MostVisitedSites::RecordOpenedMostVisitedItem(int index,
                                                   int tile_type,
                                                   int source) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", index, num_sites_);

  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s", GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, index, num_sites_);

  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.TileTypeClicked", tile_type, NUM_TILE_TYPES);

  histogram = base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                                 GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
}

void MostVisitedSites::OnBlockedSitesChanged() {
  BuildCurrentSuggestions();
}

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNumPersonalSuggestions, 0);
  // TODO(treib): Remove after M55.
  registry->RegisterListPref(prefs::kDeprecatedNTPSuggestionsURL);
  registry->RegisterListPref(prefs::kDeprecatedNTPSuggestionsIsPersonal);
}

void MostVisitedSites::BuildCurrentSuggestions() {
  // Get the current suggestions from cache. If the cache is empty, this will
  // fall back to TopSites.
  OnSuggestionsProfileAvailable(
      suggestions_service_->GetSuggestionsDataFromCache());
}

void MostVisitedSites::InitiateTopSitesQuery() {
  if (!top_sites_)
    return;
  top_sites_->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

base::FilePath MostVisitedSites::GetWhitelistLargeIconPath(const GURL& url) {
  for (const auto& whitelist : supervisor_->whitelists()) {
    if (AreURLsEquivalent(whitelist.entry_point, url))
      return whitelist.large_icon_path;
  }
  return base::FilePath();
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  SuggestionsVector suggestions;
  size_t num_tiles =
      std::min(visited_list.size(), static_cast<size_t>(num_sites_));
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      num_tiles = i;
      break;  // This is the signal that there are no more real visited sites.
    }
    if (supervisor_->IsBlocked(visited.url))
      continue;

    Suggestion suggestion;
    suggestion.title = visited.title;
    suggestion.url = visited.url;
    suggestion.source = TOP_SITES;
    suggestion.whitelist_icon_path = GetWhitelistLargeIconPath(visited.url);

    suggestions.push_back(std::move(suggestion));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = TOP_SITES;
  SaveNewSuggestions(std::move(suggestions));
  NotifyMostVisitedURLsObserver();
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    const SuggestionsProfile& suggestions_profile) {
  int num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local TopSites.
  if (num_tiles == 0 ||
      !base::FeatureList::IsEnabled(kDisplaySuggestionsServiceTiles)) {
    InitiateTopSitesQuery();
    return;
  }
  if (num_sites_ < num_tiles)
    num_tiles = num_sites_;

  SuggestionsVector suggestions;
  for (int i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion_pb = suggestions_profile.suggestions(i);
    GURL url(suggestion_pb.url());
    if (supervisor_->IsBlocked(url))
      continue;

    Suggestion suggestion;
    suggestion.title = base::UTF8ToUTF16(suggestion_pb.title());
    suggestion.url = url;
    suggestion.source = SUGGESTIONS_SERVICE;
    suggestion.whitelist_icon_path = GetWhitelistLargeIconPath(url);

    suggestions.push_back(std::move(suggestion));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = SUGGESTIONS_SERVICE;
  SaveNewSuggestions(std::move(suggestions));
  NotifyMostVisitedURLsObserver();
}

MostVisitedSites::SuggestionsVector
MostVisitedSites::CreateWhitelistEntryPointSuggestions(
    const SuggestionsVector& personal_suggestions) {
  size_t num_personal_suggestions = personal_suggestions.size();
  DCHECK_LE(num_personal_suggestions, static_cast<size_t>(num_sites_));

  size_t num_whitelist_suggestions = num_sites_ - num_personal_suggestions;
  SuggestionsVector whitelist_suggestions;

  std::set<std::string> personal_hosts;
  for (const auto& suggestion : personal_suggestions)
    personal_hosts.insert(suggestion.url.host());

  for (const auto& whitelist : supervisor_->whitelists()) {
    // Skip blacklisted sites.
    if (top_sites_ && top_sites_->IsBlacklisted(whitelist.entry_point))
      continue;

    // Skip suggestions already present.
    if (personal_hosts.find(whitelist.entry_point.host()) !=
        personal_hosts.end())
      continue;

    // Skip whitelist entry points that are manually blocked.
    if (supervisor_->IsBlocked(whitelist.entry_point))
      continue;

    Suggestion suggestion;
    suggestion.title = whitelist.title;
    suggestion.url = whitelist.entry_point;
    suggestion.source = WHITELIST;
    suggestion.whitelist_icon_path = whitelist.large_icon_path;

    whitelist_suggestions.push_back(std::move(suggestion));
    if (whitelist_suggestions.size() >= num_whitelist_suggestions)
      break;
  }

  return whitelist_suggestions;
}

MostVisitedSites::SuggestionsVector
MostVisitedSites::CreatePopularSitesSuggestions(
    const SuggestionsVector& personal_suggestions,
    const SuggestionsVector& whitelist_suggestions) {
  // For child accounts popular sites suggestions will not be added.
  if (supervisor_->IsChildProfile())
    return SuggestionsVector();

  size_t num_suggestions =
      personal_suggestions.size() + whitelist_suggestions.size();
  DCHECK_LE(num_suggestions, static_cast<size_t>(num_sites_));

  // Collect non-blacklisted popular suggestions, skipping those already present
  // in the personal suggestions.
  size_t num_popular_sites_suggestions = num_sites_ - num_suggestions;
  SuggestionsVector popular_sites_suggestions;

  if (num_popular_sites_suggestions > 0 && popular_sites_) {
    std::set<std::string> hosts;
    for (const auto& suggestion : personal_suggestions)
      hosts.insert(suggestion.url.host());
    for (const auto& suggestion : whitelist_suggestions)
      hosts.insert(suggestion.url.host());
    for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
      // Skip blacklisted sites.
      if (top_sites_ && top_sites_->IsBlacklisted(popular_site.url))
        continue;
      std::string host = popular_site.url.host();
      // Skip suggestions already present in personal or whitelists.
      if (hosts.find(host) != hosts.end())
        continue;

      Suggestion suggestion;
      suggestion.title = popular_site.title;
      suggestion.url = GURL(popular_site.url);
      suggestion.source = POPULAR;

      popular_sites_suggestions.push_back(std::move(suggestion));
      if (popular_sites_suggestions.size() >= num_popular_sites_suggestions)
        break;
    }
  }
  return popular_sites_suggestions;
}

void MostVisitedSites::SaveNewSuggestions(
    SuggestionsVector personal_suggestions) {
  SuggestionsVector whitelist_suggestions =
      CreateWhitelistEntryPointSuggestions(personal_suggestions);
  SuggestionsVector popular_sites_suggestions =
      CreatePopularSitesSuggestions(personal_suggestions,
                                    whitelist_suggestions);

  size_t num_actual_tiles = personal_suggestions.size() +
                            whitelist_suggestions.size() +
                            popular_sites_suggestions.size();
  DCHECK_LE(num_actual_tiles, static_cast<size_t>(num_sites_));

  current_suggestions_ = MergeSuggestions(std::move(personal_suggestions),
                                          std::move(whitelist_suggestions),
                                          std::move(popular_sites_suggestions));
  DCHECK_EQ(num_actual_tiles, current_suggestions_.size());

  int num_personal_suggestions = 0;
  for (const auto& suggestion : current_suggestions_) {
    if (suggestion.source != POPULAR)
      num_personal_suggestions++;
  }
  prefs_->SetInteger(prefs::kNumPersonalSuggestions, num_personal_suggestions);
  // TODO(treib): Remove after M55.
  prefs_->ClearPref(prefs::kDeprecatedNTPSuggestionsIsPersonal);
  prefs_->ClearPref(prefs::kDeprecatedNTPSuggestionsURL);
}

// static
MostVisitedSites::SuggestionsVector MostVisitedSites::MergeSuggestions(
    SuggestionsVector personal_suggestions,
    SuggestionsVector whitelist_suggestions,
    SuggestionsVector popular_suggestions) {
  SuggestionsVector merged_suggestions;
  std::move(personal_suggestions.begin(), personal_suggestions.end(),
            std::back_inserter(merged_suggestions));
  std::move(whitelist_suggestions.begin(), whitelist_suggestions.end(),
            std::back_inserter(merged_suggestions));
  std::move(popular_suggestions.begin(), popular_suggestions.end(),
            std::back_inserter(merged_suggestions));
  return merged_suggestions;
}

void MostVisitedSites::NotifyMostVisitedURLsObserver() {
  if (!waiting_for_most_visited_sites_ && !waiting_for_popular_sites_ &&
      !recorded_uma_) {
    RecordImpressionUMAMetrics();
    recorded_uma_ = true;
  }

  if (!observer_)
    return;

  observer_->OnMostVisitedURLsAvailable(current_suggestions_);
}

void MostVisitedSites::OnPopularSitesAvailable(bool success) {
  waiting_for_popular_sites_ = false;

  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  // Pass the popular sites to the observer. This will cause it to fetch any
  // missing icons, but will *not* cause it to display the popular sites.
  observer_->OnPopularURLsAvailable(popular_sites_->sites());

  // Re-build the suggestions list. Once done, this will notify the observer.
  BuildCurrentSuggestions();
}

void MostVisitedSites::RecordImpressionUMAMetrics() {
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles",
                              current_suggestions_.size());

  for (size_t i = 0; i < current_suggestions_.size(); i++) {
    UMA_HISTOGRAM_ENUMERATION(
        "NewTabPage.SuggestionsImpression", static_cast<int>(i), num_sites_);

    std::string histogram = base::StringPrintf(
        "NewTabPage.SuggestionsImpression.%s",
        GetSourceHistogramName(current_suggestions_[i].source).c_str());
    LogHistogramEvent(histogram, static_cast<int>(i), num_sites_);
  }
}

void MostVisitedSites::TopSitesLoaded(TopSites* top_sites) {}

void MostVisitedSites::TopSitesChanged(TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == TOP_SITES) {
    // The displayed suggestions are invalidated.
    InitiateTopSitesQuery();
  }
}

}  // namespace ntp_tiles
