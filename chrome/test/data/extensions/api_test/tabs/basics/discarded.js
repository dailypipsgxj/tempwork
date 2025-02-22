// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTab;
chrome.test.runTests([

    function setupWindow() {
      var testTabId;

      createWindow(["about:blank", "chrome://newtab/"], {},
                   pass(function(winId, tabIds) {
        testTabId = tabIds[1];
      }));

      waitForAllTabs(pass(function() {
        queryForTab(testTabId, {}, pass(function(tab) {
          testTab = tab;
        }));
      }));
    },

    function discard() {
      // Initially tab isn't discarded.
      assertFalse(testTab.discarded);

      var onUpdatedCompleted = chrome.test.listenForever(
          chrome.tabs.onUpdated,
          function(tabId, changeInfo, tab) {
        if ('discarded' in changeInfo) {
          // Make sure it's the right tab.
          assertEq(testTab.index, tab.index);
          assertEq(testTab.windowId, tab.windowId);

          // Make sure the discarded state changed correctly.
          assertTrue(changeInfo.discarded);
          assertTrue(tab.discarded);

          onUpdatedCompleted();
        }
      });

      // TODO(georgesak): Remove tab update when http://crbug.com/632839 is
      // resolved.
      // Discard and update testTab (the id changes after a tab is discarded).
      chrome.tabs.discard(testTab.id, pass(function(tab) {
        assertTrue(tab.discarded);
        testTab = tab;
      }));
    },

  function reload() {
    // Tab is already discarded.
    assertTrue(testTab.discarded);

    var onUpdatedCompleted = chrome.test.listenForever(
        chrome.tabs.onUpdated,
        function(tabId, changeInfo, tab) {
      if ('discarded' in changeInfo) {
        // Make sure it's the right tab.
        assertEq(testTab.index, tab.index);
        assertEq(testTab.windowId, tab.windowId);

        // Make sure the discarded state changed correctly.
        assertFalse(changeInfo.discarded);
        assertFalse(tab.discarded);

        onUpdatedCompleted();
      }
    });

    chrome.tabs.reload(testTab.id);
  }
]);
