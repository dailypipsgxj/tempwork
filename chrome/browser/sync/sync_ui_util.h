// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "base/strings/string16.h"
#include "build/build_config.h"

class Profile;
class ProfileSyncService;
class SigninManagerBase;

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.
namespace sync_ui_util {

enum MessageType {
  PRE_SYNCED,  // User has not set up sync.
  SYNCED,      // We are synced and authenticated to a gmail account.
  SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
  SYNC_PROMO,  // A situation has occurred which should be brought to the user's
               // attention, but not as an error.
};

enum StatusLabelStyle {
  PLAIN_TEXT,  // Label will be plain-text only.
  WITH_HTML    // Label may contain an HTML-formatted link.
};

// Sync errors that should be exposed to the user through the avatar button.
enum AvatarSyncErrorType {
  NO_SYNC_ERROR,                     // No sync error.
  MANAGED_USER_UNRECOVERABLE_ERROR,  // Unrecoverable error for managed users.
  UNRECOVERABLE_ERROR,               // Unrecoverable error for regular users.
  AUTH_ERROR,                        // Authentication error.
  UPGRADE_CLIENT_ERROR,              // Out-of-date client error.
  PASSPHRASE_ERROR                   // Sync passphrase error.
};

// TODO(akalin): audit the use of ProfileSyncService* service below,
// and use const ProfileSyncService& service where possible.

// Create status and link labels for the current status labels and link text
// by querying |service|.
// |style| sets the link properties, see |StatusLabelStyle|.
MessageType GetStatusLabels(Profile* profile,
                            ProfileSyncService* service,
                            const SigninManagerBase& signin,
                            StatusLabelStyle style,
                            base::string16* status_label,
                            base::string16* link_label);

// Same as above but for use specifically on the New Tab Page.
// |status_label| may contain an HTML-formatted link.
MessageType GetStatusLabelsForNewTabPage(Profile* profile,
                                         ProfileSyncService* service,
                                         const SigninManagerBase& signin,
                                         base::string16* status_label,
                                         base::string16* link_label);

#if !defined(OS_CHROMEOS)
// Gets various labels for the sync global error based on the sync error state.
// |menu_item_label|, |bubble_message|, and |bubble_accept_label| must not be
// NULL. Note that we don't use SyncGlobalError on Chrome OS.
void GetStatusLabelsForSyncGlobalError(const ProfileSyncService* service,
                                       base::string16* menu_item_label,
                                       base::string16* bubble_message,
                                       base::string16* bubble_accept_label);

// Gets the error message and button label for the sync errors that should be
// exposed to the user through the titlebar avatar button.
AvatarSyncErrorType GetMessagesForAvatarSyncError(Profile* profile,
                                                  int* content_string_id,
                                                  int* button_string_id);
#endif

MessageType GetStatus(Profile* profile,
                      ProfileSyncService* service,
                      const SigninManagerBase& signin);

}  // namespace sync_ui_util
#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
