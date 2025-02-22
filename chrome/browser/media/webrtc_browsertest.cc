// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/public/common/features.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

static const char kKeygenAlgorithmRsa[] =
    "{ name: \"RSASSA-PKCS1-v1_5\", modulusLength: 2048, publicExponent: "
    "new Uint8Array([1, 0, 1]), hash: \"SHA-256\" }";
static const char kKeygenAlgorithmEcdsa[] =
    "{ name: \"ECDSA\", namedCurve: \"P-256\" }";

// Top-level integration test for WebRTC. It always uses fake devices; see
// WebRtcWebcamBrowserTest for a test that acquires any real webcam on the
// system.
class WebRtcBrowserTest : public WebRtcTestBase {
 public:
  WebRtcBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Always use fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Flag used by TestWebAudioMediaStream to force garbage collection.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

  void RunsAudioVideoWebRTCCallInTwoTabs(
      const std::string& video_codec = WebRtcTestBase::kUseDefaultVideoCodec,
      const std::string& offer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen,
      const std::string& answer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    SetupPeerconnectionWithLocalStream(left_tab_, offer_cert_keygen_alg);
    SetupPeerconnectionWithLocalStream(right_tab_, answer_cert_keygen_alg);

    NegotiateCall(left_tab_, right_tab_, video_codec);

    DetectVideoAndHangUp();
  }

  void RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(
      const std::string& cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    // Generate and clone a certificate, resulting in JavaScript variable
    // |gCertificateClone| being set to the resulting clone.
    DeleteDatabase(left_tab_);
    OpenDatabase(left_tab_);
    GenerateAndCloneCertificate(left_tab_, cert_keygen_alg);
    CloseDatabase(left_tab_);
    DeleteDatabase(left_tab_);

    SetupPeerconnectionWithCertificateAndLocalStream(
        left_tab_, "gCertificateClone");
    SetupPeerconnectionWithLocalStream(right_tab_, cert_keygen_alg);

    NegotiateCall(left_tab_, right_tab_, WebRtcTestBase::kUseDefaultVideoCodec);

    DetectVideoAndHangUp();
  }

protected:
  void StartServerAndOpenTabs() {
    ASSERT_TRUE(embedded_test_server()->Start());
    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  }

  void DetectVideoAndHangUp() {
    StartDetectingVideo(left_tab_, "remote-view");
    StartDetectingVideo(right_tab_, "remote-view");
#if !defined(OS_MACOSX)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(left_tab_);
    WaitForVideoToPlay(right_tab_);
#endif
    HangUp(left_tab_);
    HangUp(right_tab_);
  }

  content::WebContents* left_tab_;
  content::WebContents* right_tab_;
};

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP8) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP8");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP9) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP9");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsH264) {
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(content::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
        "Skipping WebRtcBrowserTest.RunsAudioVideoWebRTCCallInTwoTabsH264 "
        "(test \"OK\")";
    return;
  }
  RunsAudioVideoWebRTCCallInTwoTabs("H264");
}

#endif  // BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, TestWebAudioMediaStream) {
  // This tests against crash regressions for the WebAudio-MediaStream
  // integration.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/webrtc/webaudio_crash.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // A sleep is necessary to be able to detect the crash.
  test::SleepInJavascript(tab, 1000);

  ASSERT_FALSE(tab->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    kKeygenAlgorithmRsa,
                                    kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    kKeygenAlgorithmEcdsa,
                                    kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateRsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(
    WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    kKeygenAlgorithmRsa,
                                    kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    kKeygenAlgorithmEcdsa,
                                    kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsGetStats) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  VerifyStatsGenerated(left_tab_);

  DetectVideoAndHangUp();
}
