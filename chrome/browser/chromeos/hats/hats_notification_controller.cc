// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_notification_controller.h"

#include "ash/common/system/system_notifier.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/hats/hats_dialog.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_state.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ash_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const char kNotificationOriginUrl[] = "chrome://hats";
const float kScale_1x = 1.0f;
const float kScale_2x = 2.0f;

// Returns true if the given |profile| interacted with HaTS by either
// dismissing the notification or taking the survey within a given threshold
// days |threshold_days|.
bool DidShowSurveyToProfileRecently(Profile* profile, int threshold_days) {
  int64_t serialized_timestamp =
      profile->GetPrefs()->GetInt64(prefs::kHatsLastInteractionTimestamp);

  base::Time previous_interaction_timestamp =
      base::Time::FromInternalValue(serialized_timestamp);
  return (previous_interaction_timestamp +
          base::TimeDelta::FromDays(threshold_days)) > base::Time::Now();
}

// Returns true if at least |threshold_days| days have passed since OOBE. This
// is an indirect measure of whether the owner has used the device for at least
// |threshold_days| days.
bool IsNewDevice(int threshold_days) {
  return chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation() <=
         base::TimeDelta::FromDays(threshold_days);
}

bool IsGoogleUser(std::string username) {
  return username.find("@google.com") != std::string::npos;
}

}  // namespace

namespace chromeos {

// static
const char HatsNotificationController::kDelegateId[] = "hats_delegate";

// static
const char HatsNotificationController::kNotificationId[] = "hats_notification";

// static
const char HatsNotificationController::kImageFetcher1xId[] =
    "hats_notification_icon_fetcher_1x";

// static
const char HatsNotificationController::kImageFetcher2xId[] =
    "hats_notification_icon_fetcher_2x";

// static
const char HatsNotificationController::kGoogleIcon1xUrl[] =
    "https://www.gstatic.com/images/branding/product/1x/googleg_48dp.png";

// static
const char HatsNotificationController::kGoogleIcon2xUrl[] =
    "https://www.gstatic.com/images/branding/product/2x/googleg_48dp.png";

// static
const int HatsNotificationController::kHatsThresholdDays = 90;

// static
const int HatsNotificationController::kHatsNewDeviceThresholdDays = 7;

HatsNotificationController::HatsNotificationController(
    Profile* profile,
    image_fetcher::ImageFetcher* image_fetcher)
    : profile_(profile),
      image_fetcher_(image_fetcher),
      weak_pointer_factory_(this) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&IsNewDevice, kHatsNewDeviceThresholdDays),
      base::Bind(&HatsNotificationController::Initialize,
                 weak_pointer_factory_.GetWeakPtr()));
}

HatsNotificationController::~HatsNotificationController() {
  network_portal_detector::GetInstance()->RemoveObserver(this);
}

void HatsNotificationController::Initialize(bool is_new_device) {
  if (is_new_device) {
    // This device has been chosen for a survey, but it is too new. Instead
    // of showing the user the survey, just mark it as completed.
    UpdateLastInteractionTime();
    return;
  }

  // Add self as an observer to be notified when an internet connection is
  // available.
  network_portal_detector::GetInstance()->AddAndFireObserver(this);
}

// static
bool HatsNotificationController::ShouldShowSurveyToProfile(Profile* profile) {
  // Do not show the survey if the HaTS feature is disabled for the device. This
  // flag is controlled by finch and is enabled only when the device has been
  // selected for the survey.
  if (!base::FeatureList::IsEnabled(features::kHappininessTrackingSystem))
    return false;

  // Do not show survey if this is a guest session.
  if (profile->IsGuestSession())
    return false;

  // Do not show the survey if the current user is not an owner.
  if (!ProfileHelper::IsOwnerProfile(profile))
    return false;

  // Do not show survey if this is an non google enterprise managed device.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() &&
      !IsGoogleUser(profile->GetProfileUserName())) {
    return false;
  }

  // Do not show survey to user if user has interacted with HaTS within the past
  // |kHatsThresholdTime| time delta.
  if (DidShowSurveyToProfileRecently(profile, kHatsThresholdDays))
    return false;

  return true;
}

// NotificationDelegate override:
std::string HatsNotificationController::id() const {
  return kDelegateId;
}

// message_center::NotificationDelegate override:
void HatsNotificationController::ButtonClick(int button_index) {
  UpdateLastInteractionTime();

  // The dialog deletes itslef on close.
  HatsDialog::CreateAndShow();

  // Remove the notification.
  g_browser_process->notification_ui_manager()->CancelById(
      id(), NotificationUIManager::GetProfileID(profile_));
}

// message_center::NotificationDelegate override:
void HatsNotificationController::Close(bool by_user) {
  if (by_user)
    UpdateLastInteractionTime();
}

// NetworkPortalDetector::Observer override:
void HatsNotificationController::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "HatsController::OnPortalDetectionCompleted(): "
          << "network=" << (network ? network->path() : "") << ", "
          << "state.status=" << state.status << ", "
          << "state.response_code=" << state.response_code;
  // Return if device is not connected to the internet.
  if (state.status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)
    return;
  // Remove self as an observer to no longer receive network change updates.
  network_portal_detector::GetInstance()->RemoveObserver(this);

  if (!image_fetcher_)
    image_fetcher_.reset(new image_fetcher::ImageFetcherImpl(
        base::MakeUnique<suggestions::ImageDecoderImpl>(),
        profile_->GetRequestContext()));

  completed_requests_ = 0;

  image_fetcher_->StartOrQueueNetworkRequest(
      kImageFetcher1xId, GURL(kGoogleIcon1xUrl),
      base::Bind(&HatsNotificationController::OnImageFetched,
                 weak_pointer_factory_.GetWeakPtr()));
  image_fetcher_->StartOrQueueNetworkRequest(
      kImageFetcher2xId, GURL(kGoogleIcon2xUrl),
      base::Bind(&HatsNotificationController::OnImageFetched,
                 weak_pointer_factory_.GetWeakPtr()));
}

void HatsNotificationController::OnImageFetched(const std::string& id,
                                                const gfx::Image& image) {
  DCHECK(id == kImageFetcher1xId || id == kImageFetcher2xId);

  completed_requests_++;
  if (!image.IsEmpty()) {
    float scale = id == kImageFetcher1xId ? kScale_1x : kScale_2x;
    icon_.AddRepresentation(gfx::ImageSkiaRep(image.AsBitmap(), scale));
  }

  // Wait for both image fetcher requests to complete.
  if (completed_requests_ < 2)
    return;

  // There needs to be an icon of atleast one scale to display the notification.
  if (!icon_.HasRepresentation(kScale_1x) &&
      !icon_.HasRepresentation(kScale_2x))
    return;

  // Create and display the notification for the user.
  std::unique_ptr<Notification> notification(CreateNotification());
  g_browser_process->notification_ui_manager()->Add(*notification, profile_);
}

Notification* HatsNotificationController::CreateNotification() {
  message_center::RichNotificationData optional;
  optional.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_TAKE_SURVEY_BUTTON)));

  return new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_HATS_NOTIFICATION_BODY),
      gfx::Image(icon_),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 ash::system_notifier::kNotifierHats),
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_HATS_NAME),
      GURL(kNotificationOriginUrl), kNotificationId, optional, this);
}

void HatsNotificationController::UpdateLastInteractionTime() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         base::Time::Now().ToInternalValue());
}

}  // namespace chromeos
