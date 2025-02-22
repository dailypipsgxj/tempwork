// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <stdint.h>

#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/sessions/sync_session_snapshot.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/sync_driver/sync_error_controller.h"
#endif  // defined(OS_CHROMEOS)

typedef GoogleServiceAuthError AuthError;

namespace sync_ui_util {

namespace {

bool IsChromeDashboardEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("ChromeDashboard");
  return group_name == "Enabled";
}

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If the user hasn't yet authenticated, an
// empty string is returned.
base::string16 GetSyncedStateStatusLabel(ProfileSyncService* service,
                                         const SigninManagerBase& signin,
                                         StatusLabelStyle style) {
  std::string user_display_name = signin.GetAuthenticatedAccountInfo().email;

#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized()) {
    // On CrOS user email is sanitized and then passed to the signin manager.
    // Original email (containing dots) is stored as "display email".
    user_display_name = user_manager::UserManager::Get()->GetUserDisplayEmail(
        AccountId::FromUserEmail(user_display_name));
  }
#endif  // defined(OS_CHROMEOS)

  base::string16 user_name = base::UTF8ToUTF16(user_display_name);

  if (!user_name.empty()) {
    if (!service || service->IsManaged()) {
      // User is signed in, but sync is disabled.
      return l10n_util::GetStringFUTF16(IDS_SIGNED_IN_WITH_SYNC_DISABLED,
                                        user_name);
    } else if (!service->IsSyncRequested()) {
      // User is signed in, but sync has been stopped.
      return l10n_util::GetStringFUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED,
                                        user_name);
    }
  }

  if (!service || !service->IsSyncActive()) {
    // User is not signed in, or sync is still initializing.
    return base::string16();
  }

  DCHECK(!user_name.empty());

  // Message may also carry additional advice with an HTML link, if acceptable.
  switch (style) {
    case PLAIN_TEXT:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          user_name);
    case WITH_HTML:
      if (IsChromeDashboardEnabled()) {
        return l10n_util::GetStringFUTF16(
            IDS_SYNC_ACCOUNT_SYNCING_TO_USER_WITH_MANAGE_LINK_NEW,
            user_name,
            base::ASCIIToUTF16(chrome::kSyncGoogleDashboardURL));
      }
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER_WITH_MANAGE_LINK,
          user_name,
          base::ASCIIToUTF16(chrome::kSyncGoogleDashboardURL));
    default:
      NOTREACHED();
      return NULL;
  }
}

void GetStatusForActionableError(
    const syncer::SyncProtocolError& error,
    base::string16* status_label) {
  DCHECK(status_label);
  switch (error.action) {
    case syncer::STOP_AND_RESTART_SYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_STOP_AND_RESTART_SYNC));
      break;
    case syncer::UPGRADE_CLIENT:
       status_label->assign(
           l10n_util::GetStringFUTF16(IDS_SYNC_UPGRADE_CLIENT,
               l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      break;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_ENABLE_SYNC_ON_ACCOUNT));
    break;
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
       status_label->assign(
           l10n_util::GetStringUTF16(IDS_SYNC_CLEAR_USER_DATA));
      break;
    default:
      NOTREACHED();
  }
}

// TODO(akalin): Write unit tests for these three functions below.

// status_label and link_label must either be both NULL or both non-NULL.
MessageType GetStatusInfo(Profile* profile,
                          ProfileSyncService* service,
                          const SigninManagerBase& signin,
                          StatusLabelStyle style,
                          base::string16* status_label,
                          base::string16* link_label) {
  DCHECK_EQ(status_label == NULL, link_label == NULL);

  MessageType result_type(SYNCED);

  if (!signin.IsAuthenticated())
    return PRE_SYNCED;

  if (!service || service->IsManaged() || service->IsFirstSetupComplete() ||
      !service->IsSyncRequested()) {
    // The order or priority is going to be: 1. Unrecoverable errors.
    // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.

    if (service && service->HasUnrecoverableError()) {
      if (status_label) {
        // Unrecoverable error is sometimes accompanied by actionable error.
        // If actionable error is set then display corresponding message,
        // otherwise show generic unrecoverable error message.
        ProfileSyncService::Status status;
        service->QueryDetailedSyncStatus(&status);
        if (ShouldShowActionOnUI(status.sync_protocol_error)) {
          GetStatusForActionableError(status.sync_protocol_error, status_label);
        } else {
          status_label->assign(l10n_util::GetStringFUTF16(
              IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
              l10n_util::GetStringUTF16(
                  IDS_SYNC_UNRECOVERABLE_ERROR_HELP_URL)));
        }
      }
      return SYNC_ERROR;
    }

    // For auth errors first check if an auth is in progress.
    if (signin.AuthInProgress()) {
      if (status_label) {
        status_label->assign(
          l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
      }
      return PRE_SYNCED;
    }

    // Check for sync errors if the sync service is enabled.
    if (service) {
      // Since there is no auth in progress, check for an auth error first.
      AuthError auth_error =
          SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
      if (auth_error.state() != AuthError::NONE) {
        if (status_label && link_label)
          signin_ui_util::GetStatusLabelsForAuthError(profile, signin,
                                                      status_label, link_label);
        return SYNC_ERROR;
      }

      // We don't have an auth error. Check for an actionable error.
      ProfileSyncService::Status status;
      service->QueryDetailedSyncStatus(&status);
      if (ShouldShowActionOnUI(status.sync_protocol_error)) {
        if (status_label) {
          GetStatusForActionableError(status.sync_protocol_error,
                                      status_label);
        }
        return SYNC_ERROR;
      }

      // Check for a passphrase error.
      if (service->IsPassphraseRequired()) {
        if (service->IsPassphraseRequiredForDecryption()) {
          // TODO(lipalani) : Ask tim if this is still needed.
          // NOT first machine.
          // Show a link ("needs attention"), but still indicate the
          // current synced status.  Return SYNC_PROMO so that
          // the configure link will still be shown.
          if (status_label && link_label) {
            status_label->assign(GetSyncedStateStatusLabel(
                service, signin, style));
            link_label->assign(
                l10n_util::GetStringUTF16(IDS_SYNC_PASSWORD_SYNC_ATTENTION));
          }
          return SYNC_PROMO;
        }
      }

      // Check to see if sync has been disabled via the dasboard and needs to be
      // set up once again.
      if (!service->IsSyncRequested() &&
          status.sync_protocol_error.error_type == syncer::NOT_MY_BIRTHDAY) {
        if (status_label) {
          status_label->assign(GetSyncedStateStatusLabel(service,
                                                         signin,
                                                         style));
        }
        return PRE_SYNCED;
      }
    }

    // There is no error. Display "Last synced..." message.
    if (status_label)
      status_label->assign(GetSyncedStateStatusLabel(service, signin, style));
    return SYNCED;
  } else {
    // Either show auth error information with a link to re-login, auth in prog,
    // or provide a link to continue with setup.
    if (service->IsFirstSetupInProgress()) {
      result_type = PRE_SYNCED;
      ProfileSyncService::Status status;
      service->QueryDetailedSyncStatus(&status);
      AuthError auth_error =
          SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS));
      }
      if (signin.AuthInProgress()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_AUTHENTICATING_LABEL));
        }
      } else if (auth_error.state() != AuthError::NONE &&
                 auth_error.state() != AuthError::TWO_FACTOR) {
        if (status_label && link_label) {
          status_label->clear();
          signin_ui_util::GetStatusLabelsForAuthError(profile, signin,
                                                      status_label, link_label);
        }
        result_type = SYNC_ERROR;
      }
    } else if (service->HasUnrecoverableError()) {
      result_type = SYNC_ERROR;
      ProfileSyncService::Status status;
      service->QueryDetailedSyncStatus(&status);
      if (ShouldShowActionOnUI(status.sync_protocol_error)) {
        if (status_label) {
          GetStatusForActionableError(status.sync_protocol_error,
              status_label);
        }
      } else if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_SETUP_ERROR));
      }
    } else if (signin.IsAuthenticated()) {
      // The user is signed in, but sync has been stopped.
      if (status_label) {
        base::string16 label = l10n_util::GetStringFUTF16(
            IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED,
            base::UTF8ToUTF16(signin.GetAuthenticatedAccountInfo().email));
        status_label->assign(label);
        result_type = PRE_SYNCED;
      }
    }
  }
  return result_type;
}

// Returns the status info for use on the new tab page, where we want slightly
// different information than in the settings panel.
MessageType GetStatusInfoForNewTabPage(Profile* profile,
                                       ProfileSyncService* service,
                                       const SigninManagerBase& signin,
                                       base::string16* status_label,
                                       base::string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);

  if (service->IsFirstSetupComplete() && service->IsPassphraseRequired()) {
    if (service->passphrase_required_reason() == syncer::REASON_ENCRYPTION) {
      // First machine migrating to passwords.  Show as a promotion.
      if (status_label && link_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(
                IDS_SYNC_NTP_PASSWORD_PROMO,
                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_NTP_PASSWORD_ENABLE));
      }
      return SYNC_PROMO;
    } else {
      // NOT first machine.
      // Show a link and present as an error ("needs attention").
      if (status_label && link_label) {
        status_label->assign(base::string16());
        link_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_CONFIGURE_ENCRYPTION));
      }
      return SYNC_ERROR;
    }
  }

  // Fallback to default.
  return GetStatusInfo(profile, service, signin, WITH_HTML, status_label,
                       link_label);
}

}  // namespace

MessageType GetStatusLabels(Profile* profile,
                            ProfileSyncService* service,
                            const SigninManagerBase& signin,
                            StatusLabelStyle style,
                            base::string16* status_label,
                            base::string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfo(profile, service, signin, style,
                                     status_label, link_label);
}

MessageType GetStatusLabelsForNewTabPage(Profile* profile,
                                         ProfileSyncService* service,
                                         const SigninManagerBase& signin,
                                         base::string16* status_label,
                                         base::string16* link_label) {
  DCHECK(status_label);
  DCHECK(link_label);
  return sync_ui_util::GetStatusInfoForNewTabPage(profile, service, signin,
                                                  status_label, link_label);
}

#if !defined(OS_CHROMEOS)
void GetStatusLabelsForSyncGlobalError(const ProfileSyncService* service,
                                       base::string16* menu_label,
                                       base::string16* bubble_message,
                                       base::string16* bubble_accept_label) {
  DCHECK(menu_label);
  DCHECK(bubble_message);
  DCHECK(bubble_accept_label);
  *menu_label = base::string16();
  *bubble_message = base::string16();
  *bubble_accept_label = base::string16();

  // Only display an error if we've completed sync setup.
  if (!service->IsFirstSetupComplete())
    return;

  // Display a passphrase error if we have one.
  if (service->IsPassphraseRequired() &&
      service->IsPassphraseRequiredForDecryption()) {
    // This is not the first machine so ask user to enter passphrase.
    *menu_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_WRENCH_MENU_ITEM);
    *bubble_message = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE);
    *bubble_accept_label = l10n_util::GetStringUTF16(
        IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_ACCEPT);
    return;
  }
}

AvatarSyncErrorType GetMessagesForAvatarSyncError(Profile* profile,
                                                  int* content_string_id,
                                                  int* button_string_id) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // The order or priority is going to be: 1. Unrecoverable errors.
  // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.
  if (service && service->HasUnrecoverableError()) {
    // An unrecoverable error is sometimes accompanied by an actionable error.
    // If an actionable error is not set to be UPGRADE_CLIENT, then show a
    // generic unrecoverable error message.
    ProfileSyncService::Status status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action != syncer::UPGRADE_CLIENT) {
      // Display different messages and buttons for managed accounts.
      if (SigninManagerFactory::GetForProfile(profile)->IsSignoutProhibited()) {
        // For a managed user, the user is directed to the signout
        // confirmation dialogue in the settings page.
        *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_MESSAGE;
        *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON;
        return MANAGED_USER_UNRECOVERABLE_ERROR;
      }
      // For a non-managed user, we sign out on the user's behalf and prompt
      // the user to sign in again.
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_BUTTON;
      return UNRECOVERABLE_ERROR;
    }
  }

  // Check for an auth error.
  SigninErrorController* signin_error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (signin_error_controller && signin_error_controller->HasError()) {
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON;
    return AUTH_ERROR;
  }

  // Check for sync errors if the sync service is enabled.
  if (service) {
    // Check for an actionable UPGRADE_CLIENT error.
    ProfileSyncService::Status status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action == syncer::UPGRADE_CLIENT) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON;
      return UPGRADE_CLIENT_ERROR;
    }

    // Check for a sync passphrase error.
    SyncErrorController* sync_error_controller =
        service->sync_error_controller();
    if (sync_error_controller && sync_error_controller->HasError()) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON;
      return PASSPHRASE_ERROR;
    }
  }

  // There is no error.
  return NO_SYNC_ERROR;
}
#endif

MessageType GetStatus(Profile* profile,
                      ProfileSyncService* service,
                      const SigninManagerBase& signin) {
  return sync_ui_util::GetStatusInfo(profile, service, signin, WITH_HTML,
                                     nullptr, nullptr);
}

base::string16 ConstructTime(int64_t time_in_int) {
  base::Time time = base::Time::FromInternalValue(time_in_int);

  // If time is null the format function returns a time in 1969.
  if (time.is_null())
    return base::string16();
  return base::TimeFormatFriendlyDateAndTime(time);
}

}  // namespace sync_ui_util
