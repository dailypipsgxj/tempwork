// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace safe_browsing {
enum class ThreatPatternType;
}

namespace subresource_filter {

using OriginSet = std::set<std::string>;

class ContentSubresourceFilterDriver;

// Controls the activation of subresource filtering for each page load in a
// WebContents and manufactures the per-frame ContentSubresourceFilterDrivers.
// TODO(melandory): Once https://crbug.com/621856 is fixed this class should
// take care of passing the activation information not only to the main frame,
// but also to the subframes.
class ContentSubresourceFilterDriverFactory
    : public base::SupportsUserData::Data,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);
  static ContentSubresourceFilterDriverFactory* FromWebContents(
      content::WebContents* web_contents);

  explicit ContentSubresourceFilterDriverFactory(
      content::WebContents* web_contents);
  ~ContentSubresourceFilterDriverFactory() override;

  void OnMainResourceMatchedSafeBrowsingBlacklist(
      const GURL& url,
      const std::vector<GURL>& redirect_urls,
      safe_browsing::ThreatPatternType threat_type);
  const OriginSet& activation_set() const { return activate_on_origins_; }
  bool ShouldActivateForURL(const GURL& url) const;
  void AddOriginOfURLToActivationSet(const GURL& url);

  ContentSubresourceFilterDriver* DriverFromFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Checks if all preconditions are fulfilled and if so, activates filtering
  // for the given |render_frame_host|. |url| is used to check web site specific
  // preconditions and should be the web URL of the page where caller is
  // intended to activate the Safe Browsing Subresource Filter.
  void ActivateForFrameHostIfNeeded(content::RenderFrameHost* render_frame_host,
                                    const GURL& url);

 private:
  friend class ContentSubresourceFilterDriverFactoryTest;
  friend class SubresourceFilterNavigationThrottleTest;

  typedef std::map<content::RenderFrameHost*,
                   std::unique_ptr<ContentSubresourceFilterDriver>>
      FrameHostToOwnedDriverMap;

  void SetDriverForFrameHostForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ContentSubresourceFilterDriver> driver);

  void CreateDriverForFrameHostIfNeeded(
      content::RenderFrameHost* render_frame_host);
  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;

  static const char kWebContentsUserDataKey[];

  FrameHostToOwnedDriverMap frame_drivers_;

  OriginSet activate_on_origins_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
