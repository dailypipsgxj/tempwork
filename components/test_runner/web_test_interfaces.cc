// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_interfaces.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/test_runner/app_banner_client.h"
#include "components/test_runner/mock_web_audio_device.h"
#include "components/test_runner/mock_web_media_stream_center.h"
#include "components/test_runner/mock_web_midi_accessor.h"
#include "components/test_runner/mock_webrtc_peer_connection_handler.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_frame_test_client.h"
#include "components/test_runner/web_view_test_client.h"
#include "components/test_runner/web_view_test_proxy.h"
#include "components/test_runner/web_widget_test_client.h"

using namespace blink;

namespace test_runner {

WebTestInterfaces::WebTestInterfaces() : interfaces_(new TestInterfaces()) {
}

WebTestInterfaces::~WebTestInterfaces() {
}

void WebTestInterfaces::SetMainView(WebView* web_view) {
  interfaces_->SetMainView(web_view);
}

void WebTestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  interfaces_->SetDelegate(delegate);
}

void WebTestInterfaces::ResetAll() {
  interfaces_->ResetAll();
}

void WebTestInterfaces::SetTestIsRunning(bool running) {
  interfaces_->SetTestIsRunning(running);
}

void WebTestInterfaces::ConfigureForTestWithURL(const WebURL& test_url,
                                                bool generate_pixels) {
  interfaces_->ConfigureForTestWithURL(test_url, generate_pixels);
}

WebTestRunner* WebTestInterfaces::TestRunner() {
  return interfaces_->GetTestRunner();
}

WebThemeEngine* WebTestInterfaces::ThemeEngine() {
  return interfaces_->GetThemeEngine();
}

TestInterfaces* WebTestInterfaces::GetTestInterfaces() {
  return interfaces_.get();
}

WebMediaStreamCenter* WebTestInterfaces::CreateMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  return new MockWebMediaStreamCenter();
}

WebRTCPeerConnectionHandler*
WebTestInterfaces::CreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  return new MockWebRTCPeerConnectionHandler(client, interfaces_.get());
}

WebMIDIAccessor* WebTestInterfaces::CreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  return new MockWebMIDIAccessor(client, interfaces_.get());
}

WebAudioDevice* WebTestInterfaces::CreateAudioDevice(double sample_rate) {
  return new MockWebAudioDevice(sample_rate);
}

std::unique_ptr<blink::WebAppBannerClient>
WebTestInterfaces::CreateAppBannerClient() {
  std::unique_ptr<AppBannerClient> client(new AppBannerClient);
  interfaces_->SetAppBannerClient(client.get());
  return std::move(client);
}

std::unique_ptr<WebFrameTestClient> WebTestInterfaces::CreateWebFrameTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base,
    WebFrameTestProxyBase* web_frame_test_proxy_base) {
  return base::WrapUnique(new WebFrameTestClient(
      interfaces_->GetTestRunner(), interfaces_->GetDelegate(),
      web_view_test_proxy_base, web_frame_test_proxy_base));
}

std::unique_ptr<WebViewTestClient> WebTestInterfaces::CreateWebViewTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base) {
  return base::WrapUnique(new WebViewTestClient(interfaces_->GetTestRunner(),
                                                web_view_test_proxy_base));
}

std::unique_ptr<WebWidgetTestClient>
WebTestInterfaces::CreateWebWidgetTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base) {
  return base::WrapUnique(new WebWidgetTestClient(interfaces_->GetTestRunner(),
                                                  web_view_test_proxy_base));
}

std::vector<blink::WebView*> WebTestInterfaces::GetWindowList() {
  std::vector<blink::WebView*> result;
  for (WebViewTestProxyBase* proxy : interfaces_->GetWindowList())
    result.push_back(proxy->web_view());
  return result;
}

}  // namespace test_runner
