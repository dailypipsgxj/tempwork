// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"

#import <AppKit/AppKit.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

class SkBitmap;

namespace {

// KeystonePromotionInfoBarDelegate -------------------------------------------

class KeystonePromotionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If there's an active tab, creates a keystone promotion delegate and adds it
  // to the InfoBarService associated with that tab.
  static void Create();

 private:
  explicit KeystonePromotionInfoBarDelegate(PrefService* prefs);
  ~KeystonePromotionInfoBarDelegate() override;

  // Sets this info bar to be able to expire.  Called a predetermined amount
  // of time after this object is created.
  void SetCanExpire() { can_expire_ = true; }

  // ConfirmInfoBarDelegate
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // The prefs to use.
  PrefService* prefs_;  // weak

  // Whether the info bar should be dismissed on the next navigation.
  bool can_expire_;

  // Used to delay the expiration of the info bar.
  base::WeakPtrFactory<KeystonePromotionInfoBarDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(KeystonePromotionInfoBarDelegate);
};

// static
void KeystonePromotionInfoBarDelegate::Create() {
  Browser* browser = chrome::GetLastActiveBrowser();
  if (!browser)
    return;
  content::WebContents* webContents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!webContents)
    return;
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(webContents);
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new KeystonePromotionInfoBarDelegate(
              Profile::FromBrowserContext(webContents->GetBrowserContext())
                  ->GetPrefs()))));
}

KeystonePromotionInfoBarDelegate::KeystonePromotionInfoBarDelegate(
    PrefService* prefs)
    : ConfirmInfoBarDelegate(),
      prefs_(prefs),
      can_expire_(false),
      weak_ptr_factory_(this) {
  const base::TimeDelta kCanExpireOnNavigationAfterDelay =
      base::TimeDelta::FromSeconds(8);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&KeystonePromotionInfoBarDelegate::SetCanExpire,
                            weak_ptr_factory_.GetWeakPtr()),
      kCanExpireOnNavigationAfterDelay);
}

KeystonePromotionInfoBarDelegate::~KeystonePromotionInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
KeystonePromotionInfoBarDelegate::GetIdentifier() const {
  return KEYSTONE_PROMOTION_INFOBAR_DELEGATE;
}

int KeystonePromotionInfoBarDelegate::GetIconId() const {
  return IDR_PRODUCT_LOGO_32;
}

bool KeystonePromotionInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return can_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

base::string16 KeystonePromotionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PROMOTE_INFOBAR_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

base::string16 KeystonePromotionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PROMOTE_INFOBAR_PROMOTE_BUTTON : IDS_PROMOTE_INFOBAR_DONT_ASK_BUTTON);
}

bool KeystonePromotionInfoBarDelegate::Accept() {
  [[KeystoneGlue defaultKeystoneGlue] promoteTicket];
  return true;
}

bool KeystonePromotionInfoBarDelegate::Cancel() {
  prefs_->SetBoolean(prefs::kShowUpdatePromotionInfoBar, false);
  return true;
}

}  // namespace


// KeystonePromotionInfoBar ---------------------------------------------------

@interface KeystonePromotionInfoBar : NSObject
- (void)checkAndShowInfoBarForProfile:(Profile*)profile;
- (void)updateStatus:(NSNotification*)notification;
- (void)removeObserver;
@end  // @interface KeystonePromotionInfoBar

@implementation KeystonePromotionInfoBar

- (void)dealloc {
  [self removeObserver];
  [super dealloc];
}

- (void)checkAndShowInfoBarForProfile:(Profile*)profile {
  // If this is the first run, the user clicked the "don't ask again" button
  // at some point in the past, or if the "don't ask about the default
  // browser" command-line switch is present, bail out.  That command-line
  // switch is recycled here because it's likely that the set of users that
  // don't want to be nagged about the default browser also don't want to be
  // nagged about the update check.  (Automated testers, I'm thinking of
  // you...)
  base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
  if (first_run::IsChromeFirstRun() ||
      !profile->GetPrefs()->GetBoolean(prefs::kShowUpdatePromotionInfoBar) ||
      commandLine->HasSwitch(switches::kNoDefaultBrowserCheck)) {
    return;
  }

  // If there is no Keystone glue (maybe because this application isn't
  // Keystone-enabled) or the application is on a read-only filesystem,
  // doing anything related to auto-update is pointless.  Bail out.
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  if (!keystoneGlue || [keystoneGlue isOnReadOnlyFilesystem]) {
    return;
  }

  // Stay alive as long as needed.  This is balanced by a release in
  // -updateStatus:.
  [self retain];

  AutoupdateStatus recentStatus = [keystoneGlue recentStatus];
  if (recentStatus == kAutoupdateNone ||
      recentStatus == kAutoupdateRegistering) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(updateStatus:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  } else {
    [self updateStatus:[keystoneGlue recentNotification]];
  }
}

- (void)updateStatus:(NSNotification*)notification {
  NSDictionary* dictionary = [notification userInfo];
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);

  if (status == kAutoupdateNone || status == kAutoupdateRegistering) {
    return;
  }

  [self removeObserver];

  if (status != kAutoupdateRegisterFailed &&
      [[KeystoneGlue defaultKeystoneGlue] needsPromotion]) {
    KeystonePromotionInfoBarDelegate::Create();
  }

  [self release];
}

- (void)removeObserver {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

@end  // @implementation KeystonePromotionInfoBar

// static
void KeystoneInfoBar::PromotionInfoBar(Profile* profile) {
  KeystonePromotionInfoBar* promotionInfoBar =
      [[[KeystonePromotionInfoBar alloc] init] autorelease];

  [promotionInfoBar checkAndShowInfoBarForProfile:profile];
}
