// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-languages-page. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
// SettingsPageBrowserTest fixture.
GEN_INCLUDE([ROOT_PATH +
             'chrome/test/data/webui/settings/settings_page_browsertest.js']);

var languageSettings = languageSettings || {};

/**
 * Test class for settings-languages-page UI.
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsLanguagesPageBrowserTest() {}

SettingsLanguagesPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/advanced',

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
  },
};

// May time out on debug builders and memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_LanguagesPage DISABLED_LanguagesPage');
GEN('#else');
GEN('#define MAYBE_LanguagesPage LanguagesPage');
GEN('#endif');

// Runs languages page tests.
TEST_F('SettingsLanguagesPageBrowserTest', 'MAYBE_LanguagesPage', function() {
  suite('languages page', function() {
    testing.Test.disableAnimationsAndTransitions();

    var advanced = this.getPage('advanced');

    var languagesSection;
    var languagesPage;
    suiteSetup(function() {
      advanced.set('pageVisibility.languages', true);
      Polymer.dom.flush();

      languagesSection = this.getSection(advanced, 'languages');
      assertTrue(!!languagesSection);
      languagesPage = languagesSection.querySelector('settings-languages-page');
      assertTrue(!!languagesPage);

      return LanguageHelperImpl.getInstance().whenReady();
    }.bind(this));

    teardown(function(done) {
      // Close the section if we're in a sub-page.
      if (advanced.currentRoute.subpage.length == 0) {
        done();
      } else {
        advanced.currentRoute = {
          page: 'advanced',
          section: '',
          subpage: [],
        };
        setTimeout(done);
      }
    });

    test('manage languages', function() {
      var manageLanguagesButton =
          languagesPage.$.languagesCollapse.querySelector(
              '.list-button:last-of-type');
      MockInteractions.tap(manageLanguagesButton);
      assertTrue(!!languagesPage.$$('settings-manage-languages-page'));
    });

    test('language detail', function() {
      var languagesCollapse = languagesPage.$.languagesCollapse;
      var languageDetailMenuItem = languagesCollapse.querySelectorAll(
          '.dropdown-content .dropdown-item')[2];
      assertTrue(!!languageDetailMenuItem);
      MockInteractions.tap(languageDetailMenuItem);

      var languageDetailPage =
          languagesPage.$$('settings-language-detail-page');
      assertTrue(!!languageDetailPage);
      assertEquals('en-US', languageDetailPage.detail.language.code);
    });

    test('manage input methods', function() {
      var inputMethodsCollapse = languagesPage.$.inputMethodsCollapse;
      var inputMethodSettingsExist = !!inputMethodsCollapse;
      if (cr.isChromeOS) {
        assertTrue(inputMethodSettingsExist);
        var manageInputMethodsButton =
            inputMethodsCollapse.querySelector('.list-button:last-of-type');
        MockInteractions.tap(manageInputMethodsButton);
        assertTrue(!!languagesPage.$$('settings-manage-input-methods-page'));
      } else {
        assertFalse(inputMethodSettingsExist);
      }
    });

    test('spellcheck', function() {
      var spellCheckCollapse = languagesPage.$.spellCheckCollapse;
      var spellCheckSettingsExist = !!spellCheckCollapse;
      if (cr.isMac) {
        assertFalse(spellCheckSettingsExist);
      } else {
        assertTrue(spellCheckSettingsExist);
        MockInteractions.tap(
            spellCheckCollapse.querySelector('.list-button:last-of-type'));
        assertTrue(!!languagesPage.$$('settings-edit-dictionary-page'));
      }
    });
  }.bind(this));

  // TODO(michaelpg): Test more aspects of the languages UI.

  // Run all registered tests.
  mocha.run();
});
