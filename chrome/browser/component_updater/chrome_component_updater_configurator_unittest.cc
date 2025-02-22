// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/configurator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {

TEST(ChromeComponentUpdaterConfiguratorTest, TestDisablePings) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "disable-pings");

  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));

  const std::vector<GURL> pingUrls = config->PingUrl();
  EXPECT_TRUE(pingUrls.empty());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestFastUpdate) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "fast-update");

  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));

  CHECK_EQ(10, config->InitialDelay());
  CHECK_EQ(6 * 60 * 60, config->NextCheckDelay());
  CHECK_EQ(1, config->StepDelay());
  CHECK_EQ(2, config->OnDemandDelay());
  CHECK_EQ(10, config->UpdateDelay());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestOverrideUrl) {
  const char overrideUrl[] = "http://0.0.0.0/";

  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());

  std::string val = "url-source";
  val.append("=");
  val.append(overrideUrl);
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, val.c_str());

  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));

  const std::vector<GURL> urls = config->UpdateUrl();

  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(overrideUrl, urls.at(0).possibly_invalid_spec());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestSwitchRequestParam) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "test-request");

  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));

  EXPECT_FALSE(config->ExtraRequestParams().empty());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestUpdaterDefaultUrl) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));
  const auto urls = config->UpdateUrl();

  // Expect the default url to be cryptographically secure.
  EXPECT_GE(urls.size(), 1u);
  EXPECT_TRUE(urls.front().SchemeIsCryptographic());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestEnabledCupSigning) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));

  EXPECT_TRUE(config->EnabledCupSigning());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestUseEncryption) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  const auto config(MakeChromeComponentUpdaterConfigurator(cmdline, nullptr));

  const auto urls = config->UpdateUrl();
  ASSERT_EQ(2u, urls.size());
  ASSERT_STREQ(kUpdaterDefaultUrl, urls[0].spec().c_str());
  ASSERT_STREQ(kUpdaterFallbackUrl, urls[1].spec().c_str());

  ASSERT_EQ(config->UpdateUrl(), config->PingUrl());

  // Use the configurator implementation to test the filtering of
  // unencrypted URLs.
  {
    const ConfiguratorImpl config(cmdline, nullptr, true);
    const auto urls = config.UpdateUrl();
    ASSERT_EQ(1u, urls.size());
    ASSERT_STREQ(kUpdaterDefaultUrl, urls[0].spec().c_str());
    ASSERT_EQ(config.UpdateUrl(), config.PingUrl());
  }

  {
    const ConfiguratorImpl config(cmdline, nullptr, false);
    const auto urls = config.UpdateUrl();
    ASSERT_EQ(2u, urls.size());
    ASSERT_STREQ(kUpdaterDefaultUrl, urls[0].spec().c_str());
    ASSERT_STREQ(kUpdaterFallbackUrl, urls[1].spec().c_str());
    ASSERT_EQ(config.UpdateUrl(), config.PingUrl());
  }
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestEnabledComponentUpdates) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(MakeChromeComponentUpdaterConfigurator(&cmdline, nullptr));
  EXPECT_TRUE(config->EnabledComponentUpdates());
}

}  // namespace component_updater
