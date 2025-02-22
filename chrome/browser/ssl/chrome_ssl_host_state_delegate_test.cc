// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOkCertFile[] = "ok_cert.pem";

const char kWWWGoogleHost[] = "www.google.com";
const char kGoogleHost[] = "google.com";
const char kExampleHost[] = "example.com";

const uint64_t kDeltaOneDayInSeconds = UINT64_C(86400);
const uint64_t kDeltaOneWeekInSeconds = UINT64_C(604800);

scoped_refptr<net::X509Certificate> GetOkCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), kOkCertFile);
}

// Helper function for setting Finch options
void SetFinchConfig(base::CommandLine* command_line, const std::string& group) {
  command_line->AppendSwitchASCII("--force-fieldtrials",
                                  "RevertCertificateErrorDecisions/" + group);
}

}  // namespace

class ChromeSSLHostStateDelegateTest : public InProcessBrowserTest {};

// ChromeSSLHostStateDelegateTest tests basic unit test functionality of the
// SSLHostStateDelegate class.  For example, tests that if a certificate is
// accepted, then it is added to queryable, and if it is revoked, it is not
// queryable. Even though it is effectively a unit test, in needs to be an
// InProcessBrowserTest because the actual functionality is provided by
// ChromeSSLHostStateDelegate which is provided per-profile.
//
// QueryPolicy unit tests the expected behavior of calling QueryPolicy on the
// SSLHostStateDelegate class after various SSL cert decisions have been made.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, QueryPolicy) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Verifying that all three of the certs we will be looking at are denied
  // before any action has been taken.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  // Verify that only kWWWGoogleHost is allowed and that the other two certs
  // being tested still are denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Simulate a user decision to allow an invalid certificate exception for
  // kExampleHost.
  state->AllowCert(kExampleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  // Verify that both kWWWGoogleHost and kExampleHost have allow exceptions
  // while kGoogleHost still is denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kExampleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
}

// HasPolicyAndRevoke unit tests the expected behavior of calling
// HasAllowException before and after calling RevokeUserAllowExceptions on the
// SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, HasPolicyAndRevoke) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);
  bool unused_value;

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);
  state->AllowCert(kExampleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  // Verify that HasAllowException correctly acknowledges that a user decision
  // has been made about kWWWGoogleHost. Then verify that HasAllowException
  // correctly identifies that the decision has been revoked.
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost));
  state->RevokeUserAllowExceptions(kWWWGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Verify that the revocation of the kWWWGoogleHost decision does not affect
  // the Allow for kExampleHost.
  EXPECT_TRUE(state->HasAllowException(kExampleHost));

  // Verify the revocation of the kWWWGoogleHost decision does not affect the
  // non-decision for kGoogleHost. Then verify that a revocation of a URL with
  // no decision has no effect.
  EXPECT_FALSE(state->HasAllowException(kGoogleHost));
  state->RevokeUserAllowExceptions(kGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kGoogleHost));
}

// Clear unit tests the expected behavior of calling Clear to forget all cert
// decision state on the SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, Clear) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);
  bool unused_value;

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  // Do a full clear, then make sure that both kWWWGoogleHost, which had a
  // decision made, and kExampleHost, which was untouched, are now in a denied
  // state.
  state->Clear();
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
  EXPECT_FALSE(state->HasAllowException(kExampleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
}

// DidHostRunInsecureContent unit tests the expected behavior of calling
// DidHostRunInsecureContent as well as HostRanInsecureContent to check if
// insecure content has been run and to mark it as such.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest,
                       DidHostRunInsecureContent) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_FALSE(state->DidHostRunInsecureContent("example.com", 42));

  state->HostRanInsecureContent("www.google.com", 42);

  EXPECT_TRUE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_FALSE(state->DidHostRunInsecureContent("example.com", 42));

  state->HostRanInsecureContent("example.com", 42);

  EXPECT_TRUE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_TRUE(state->DidHostRunInsecureContent("example.com", 42));
}

// Test the migration code needed as a result of changing how the content
// setting is stored. We used to map the settings dictionary to the pattern
// pair <origin, origin> but now we map it to <origin, wildcard>.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, Migrate) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  // Move the new-format setting (<origin, wildcard>) to be an old-format one
  // (<origin, origin>).
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  GURL url(std::string("https://") + kWWWGoogleHost);
  std::unique_ptr<base::Value> new_format =
      map->GetWebsiteSetting(url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                             std::string(), nullptr);
  // Delete the new-format setting.
  map->SetWebsiteSettingDefaultScope(url, GURL(),
                                     CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                                     std::string(), nullptr);

  // No exception should exist.
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost));
  // Create the old-format one.
  map->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(url),
      CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(),
      std::move(new_format));

  // Test that the old-format setting works.
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost));

  // Trigger the migration code that happens on construction.
  {
    std::unique_ptr<ChromeSSLHostStateDelegate> temp_delegate(
        new ChromeSSLHostStateDelegate(profile));
  }

  // Test that the new style setting still works.
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost));

  // Check that the old-format setting is removed and only the new one exists.
  ContentSettingsForOneType settings;
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                             std::string(), &settings);
  EXPECT_EQ(1u, settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(url),
            settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), settings[0].secondary_pattern);
}

class ForgetAtSessionEndSSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    SetFinchConfig(command_line, "Session");
  }
};

// QueryPolicyExpired unit tests to make sure that if a certificate decision has
// expired, the return value from QueryPolicy returns the correct vaule.
IN_PROC_BROWSER_TEST_F(ForgetAtSessionEndSSLHostStateDelegateTest,
                       PRE_QueryPolicyExpired) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool expired_previous_decision;

  // The certificate has never been seen before, so it should be UNKNOWN and
  // should also indicate that it hasn't expired.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);

  // After allowing the certificate, a query should say that it is allowed and
  // also specify that it hasn't expired.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);
}

// Since this is being checked on a browser instance that expires security
// decisions after restart, the test needs to  wait until after a restart to
// verify that the expiration state is correct.
IN_PROC_BROWSER_TEST_F(ForgetAtSessionEndSSLHostStateDelegateTest,
                       QueryPolicyExpired) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool expired_previous_decision;

  // The browser content has restart thus expiring the user decision made above,
  // so it should indicate that the certificate and error are DENIED but also
  // that they expired since the last query.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_TRUE(expired_previous_decision);

  // However, with a new query, it should indicate that no new expiration has
  // occurred.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);
}

// Tests the basic behavior of cert memory in incognito.
class IncognitoSSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    SetFinchConfig(command_line, "OneWeek");
  }
};

IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Add a cert exception to the profile and then verify that it still exists
  // in the incognito profile.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);

  std::unique_ptr<Profile> incognito(profile->CreateOffTheRecordProfile());
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();

  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      incognito_state->QueryPolicy(
          kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Add a cert exception to the incognito profile. It will be checked after
  // restart that this exception does not exist. Note the different cert URL and
  // error than above thus mapping to a second exception. Also validate that it
  // was not added as an exception to the regular profile.
  incognito_state->AllowCert(kGoogleHost, *cert,
                             net::CERT_STATUS_COMMON_NAME_INVALID);

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kGoogleHost, *cert,
                         net::CERT_STATUS_COMMON_NAME_INVALID, &unused_value));
}

// AfterRestart ensures that any cert decisions made in an incognito profile are
// forgetten after a session restart even if the field trial group parameter
// specifies to remember cert decisions after restart.
IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Verify that the exception added before restart to the regular
  // (non-incognito) profile still exists and was not cleared after the
  // incognito session ended.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  std::unique_ptr<Profile> incognito(profile->CreateOffTheRecordProfile());
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the incognito profile was
  // cleared when the incognito session ended.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            incognito_state->QueryPolicy(kGoogleHost, *cert,
                                         net::CERT_STATUS_COMMON_NAME_INVALID,
                                         &unused_value));
}

// Tests the default certificate memory, which is one week.
class DefaultMemorySSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {};

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
}

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  ChromeSSLHostStateDelegate* chrome_state =
      static_cast<ChromeSSLHostStateDelegate*>(state);
  chrome_state->SetClock(std::unique_ptr<base::Clock>(clock));

  // Start the clock at standard system time.
  clock->SetNow(base::Time::NowFromSystemTime());

  // This should only pass if the cert was allowed before the test was restart
  // and thus has now been rememebered across browser restarts.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Simulate the clock advancing by one day, which is less than the expiration
  // length.
  clock->Advance(base::TimeDelta::FromSeconds(kDeltaOneDayInSeconds + 1));

  // The cert should still be |ALLOWED| because the default expiration length
  // has not passed yet.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));

  // Now simulate the clock advancing by one week, which is past the expiration
  // point.
  clock->Advance(base::TimeDelta::FromSeconds(kDeltaOneWeekInSeconds -
                                              kDeltaOneDayInSeconds + 1));

  // The cert should now be |DENIED| because the specified delta has passed.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
}

// The same test as ChromeSSLHostStateDelegateTest.QueryPolicyExpired but now
// applied to a browser context that expires based on time, not restart. This
// unit tests to make sure that if a certificate decision has expired, the
// return value from QueryPolicy returns the correct vaule.
IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       QueryPolicyExpired) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool expired_previous_decision;

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  ChromeSSLHostStateDelegate* chrome_state =
      static_cast<ChromeSSLHostStateDelegate*>(state);
  chrome_state->SetClock(std::unique_ptr<base::Clock>(clock));

  // Start the clock at standard system time but do not advance at all to
  // emphasize that instant forget works.
  clock->SetNow(base::Time::NowFromSystemTime());

  // The certificate has never been seen before, so it should be UNKONWN and
  // should also indicate that it hasn't expired.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);

  // After allowing the certificate, a query should say that it is allowed and
  // also specify that it hasn't expired.
  state->AllowCert(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);

  // Simulate the clock advancing by one week, the default expiration time.
  clock->Advance(base::TimeDelta::FromSeconds(kDeltaOneWeekInSeconds + 1));

  // The decision expiration time has come, so it should indicate that the
  // certificate and error are DENIED but also that they expired since the last
  // query.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_TRUE(expired_previous_decision);

  // However, with a new query, it should indicate that no new expiration has
  // occurred.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID,
                         &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);
}

// Tests to make sure that if the user deletes their browser history, SSL
// exceptions will be deleted as well.
class RemoveBrowsingHistorySSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 public:
  void RemoveAndWait(Profile* profile) {
    BrowsingDataRemover* remover =
        BrowsingDataRemoverFactory::GetForBrowserContext(profile);
    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->Remove(BrowsingDataRemover::Period(browsing_data::LAST_HOUR),
                    BrowsingDataRemover::REMOVE_HISTORY,
                    BrowsingDataHelper::UNPROTECTED_WEB);
    completion_observer.BlockUntilCompletion();
  }
};

IN_PROC_BROWSER_TEST_F(RemoveBrowsingHistorySSLHostStateDelegateTest,
                       DeleteHistory) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Add an exception for an invalid certificate. Then remove the last hour's
  // worth of browsing history and verify that the exception has been deleted.
  state->AllowCert(kGoogleHost, *cert, net::CERT_STATUS_DATE_INVALID);
  RemoveAndWait(profile);
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert,
                               net::CERT_STATUS_DATE_INVALID, &unused_value));
}

// Tests to make sure that localhost certificate errors are treated as
// normal errors or ignored, depending on whether the
// kAllowInsecureLocalhost flag is set.
//
// When the flag isn't set, requests to localhost with invalid
// certificates should be denied.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest,
                       LocalhostErrorWithoutFlag) {
  // Serve the Google cert for localhost to generate an error.
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy("localhost", *cert,
                         net::CERT_STATUS_COMMON_NAME_INVALID, &unused_value));

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy("127.0.0.1", *cert,
                         net::CERT_STATUS_COMMON_NAME_INVALID, &unused_value));
}

// When the flag is set, requests to localhost with invalid certificates
// should be allowed.
class AllowLocalhostErrorsSSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

IN_PROC_BROWSER_TEST_F(AllowLocalhostErrorsSSLHostStateDelegateTest,
                       LocalhostErrorWithFlag) {
  // Serve the Google cert for localhost to generate an error.
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy("localhost", *cert,
                         net::CERT_STATUS_COMMON_NAME_INVALID, &unused_value));

  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy("127.0.0.1", *cert,
                         net::CERT_STATUS_COMMON_NAME_INVALID, &unused_value));
}
