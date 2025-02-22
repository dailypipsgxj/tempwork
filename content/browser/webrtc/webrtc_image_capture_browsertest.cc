// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

static const char kImageCaptureHtmlFile[] = "/media/image_capture_test.html";

// TODO(mcasas): enable real-camera tests by disabling the Fake Device for
// platforms where the ImageCaptureCode is landed, https://crbug.com/518807.
static struct TargetCamera {
  bool use_fake;
} const kTestParameters[] = {{true}};

}  // namespace

namespace content {

#if defined(OS_WIN)
// This test is flaky on WebRTC Windows bots: https://crbug.com/633242.
#define MAYBE_WebRtcImageCaptureBrowserTest \
  DISABLED_WebRtcImageCaptureBrowserTest
#else
#define MAYBE_WebRtcImageCaptureBrowserTest WebRtcImageCaptureBrowserTest
#endif

// This class is the content_browsertests for Image Capture API, which allows
// for capturing still images out of a MediaStreamTrack. Is a
// WebRtcWebcamBrowserTest to be able to use a physical camera.
class MAYBE_WebRtcImageCaptureBrowserTest
    : public WebRtcWebcamBrowserTest,
      public testing::WithParamInterface<struct TargetCamera> {
 public:
  MAYBE_WebRtcImageCaptureBrowserTest() = default;
  ~MAYBE_WebRtcImageCaptureBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    if (GetParam().use_fake) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeDeviceForMediaStream);
      ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream));
    }

    // Enables promised-based navigator.mediaDevices.getUserMedia();
    // TODO(mcasas): remove after https://crbug.com/503227 is closed.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");

    // Specific flag to enable ImageCapture API.
    // TODO(mcasas): remove after https://crbug.com/603328 is closed.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "ImageCapture");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_WebRtcImageCaptureBrowserTest);
};

IN_PROC_BROWSER_TEST_P(MAYBE_WebRtcImageCaptureBrowserTest,
                       CreateAndGetCapabilities) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kImageCaptureHtmlFile));
  NavigateToURL(shell(), url);

  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), "testCreateAndGetCapabilities()", &result));
  if (result == "OK")
    return;
  FAIL();
}

INSTANTIATE_TEST_CASE_P(,
                        MAYBE_WebRtcImageCaptureBrowserTest,
                        testing::ValuesIn(kTestParameters));

}  // namespace content
