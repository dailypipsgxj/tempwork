// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_

#include <windows.h>
#include <winhttp.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "net/proxy/polling_proxy_config_service.h"

namespace base {
namespace win {
class RegKey;
}
}  // namespace base.

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings.
//
// It works by calling WinHttpGetIEProxyConfigForCurrentUser() to fetch the
// Internet Explorer proxy settings.
//
// We use two different strategies to notice when the configuration has
// changed:
//
// (1) Watch the internet explorer settings registry keys for changes. When
//     one of the registry keys pertaining to proxy settings has changed, we
//     call WinHttpGetIEProxyConfigForCurrentUser() again to read the
//     configuration's new value.
//
// (2) Do regular polling every 10 seconds during network activity to see if
//     WinHttpGetIEProxyConfigForCurrentUser() returns something different.
//
// Ideally strategy (1) should be sufficient to pick up all of the changes.
// However we still do the regular polling as a precaution in case the
// implementation details of  WinHttpGetIEProxyConfigForCurrentUser() ever
// change, or in case we got it wrong (and are not checking all possible
// registry dependencies).
class NET_EXPORT_PRIVATE ProxyConfigServiceWin
    : public PollingProxyConfigService {
 public:
  ProxyConfigServiceWin();
  ~ProxyConfigServiceWin() override;

  // Overrides a function from PollingProxyConfigService.
  void AddObserver(Observer* observer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProxyConfigServiceWinTest, SetFromIEConfig);
  typedef std::vector<base::win::RegKey*> RegKeyList;

  // Registers change observers on the registry keys relating to proxy settings.
  void StartWatchingRegistryForChanges();

  // Creates a new key and appends it to |keys_to_watch_|. If the key fails to
  // be created, it is not appended to the list and we return false.
  bool AddKeyToWatchList(HKEY rootkey, const wchar_t* subkey);

  // This is called whenever one of the registry keys we are watching change.
  void OnObjectSignaled(base::win::RegKey* key);

  static void GetCurrentProxyConfig(ProxyConfig* config);

  // Set |config| using the proxy configuration values of |ie_config|.
  static void SetFromIEConfig(
      ProxyConfig* config,
      const WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& ie_config);

  RegKeyList keys_to_watch_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_WIN_H_
