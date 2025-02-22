// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

// Fake data generator.
GEN_INCLUDE([ROOT_PATH +
    'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsPasswordSectionBrowserTest() {}

SettingsPasswordSectionBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/passwords_and_forms_page/passwords_section.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');
  },

  /**
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
   *     expected data.
   * @private
   */
  validatePasswordList: function(nodes, passwordList) {
    assertEquals(passwordList.length, nodes.length);
    for (var index = 0; index < passwordList.length; ++index) {
      var node = nodes[index];
      var passwordInfo = passwordList[index];
      assertEquals(passwordInfo.loginPair.originUrl,
                   node.querySelector('#originUrl').textContent);
      assertEquals(passwordInfo.linkUrl,
                   node.querySelector('#originUrl').href);
      assertEquals(passwordInfo.loginPair.username,
                   node.querySelector('#username').textContent);
      assertEquals(passwordInfo.numCharactersInPassword,
                   node.querySelector('#password').value.length);
    }
  },

  /**
   * Helper method that validates a that elements in the exception list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList The
   *     expected data.
   * @private
   */
  validateExceptionList_: function(nodes, exceptionList) {
    assertEquals(exceptionList.length, nodes.length);
    for (var index = 0; index < exceptionList.length; ++index) {
      var node = nodes[index];
      var exception = exceptionList[index];
      assertEquals(exception.exceptionUrl,
                   node.querySelector('#exception').textContent);
      assertEquals(exception.linkUrl,
                   node.querySelector('#exception').href);
    }
  },

  /**
   * Returns all children of an element that has children added by a dom-repeat.
   * @param {!Element} element
   * @return {!Array<!Element>}
   * @private
   */
  getDomRepeatChildren_: function(element) {
    var template = element.children[element.children.length - 1];
    assertEquals('TEMPLATE', template.tagName);
    // The template is at the end of the list of children and should be skipped.
    return Array.prototype.slice.call(element.children, 0, -1);
  },

  /**
   * Skips over the template and returns all visible children of an iron-list.
   * @param {!Element} ironList
   * @return {!Array<!Element>}
   * @private
   */
  getIronListChildren_: function(ironList) {
    return Polymer.dom(ironList).children.filter(function(child, i) {
      return i != 0 && !child.hidden;
    });
  },

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList
   * @return {!Object}
   * @private
   */
  createPasswordsSection_: function(passwordList, exceptionList) {
    // Create a passwords-section to use for testing.
    var passwordsSection = document.createElement('passwords-section');
    passwordsSection.savedPasswords = passwordList;
    passwordsSection.passwordExceptions = exceptionList;
    document.body.appendChild(passwordsSection);
    this.flushPasswordSection_(passwordsSection);
    return passwordsSection;
  },

  /**
   * Helper method used to create a password editing dialog.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordItem
   * @return {!Object}
   * @private
   */
  createPasswordDialog_: function(passwordItem) {
    var passwordDialog = document.createElement('password-edit-dialog');
    passwordDialog.item = passwordItem;
    document.body.appendChild(passwordDialog);
    Polymer.dom.flush();
    return passwordDialog;
  },

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {string} url The URL that is being searched for.
   */
  listContainsUrl(passwordList, url) {
    for (var i = 0; i < passwordList.length; ++i) {
      if (passwordList[i].loginPair.originUrl == url)
        return true;
    }
    return false;
  },

  /**
   * Helper method used to test for a url in a list of passwords.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionPair>} exceptionList
   * @param {string} url The URL that is being searched for.
   */
  exceptionsListContainsUrl(exceptionList, url) {
    for (var i = 0; i < exceptionList.length; ++i) {
      if (exceptionList[i].exceptionUrl == url)
        return true;
    }
    return false;
  },

  /**
   * Allow the iron-list to be sized properly.
   * @param {!Object} passwordsSection
   * @private
   */
  flushPasswordSection_: function(passwordsSection) {
    passwordsSection.$.passwordList.notifyResize();
    Polymer.dom.flush();
  },
};

/**
 * This test will validate that the section is loaded with data.
 */
TEST_F('SettingsPasswordSectionBrowserTest', 'uiTests', function() {
  var self = this;

  suite('PasswordsSection', function() {
    test('verifySavedPasswordLength', function() {
      assertEquals(self.browsePreload, document.location.href);

      var passwordList = [
        FakeDataMaker.passwordEntry('site1.com', 'luigi', 1),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7),
        FakeDataMaker.passwordEntry('site2.com', 'mario', 70),
        FakeDataMaker.passwordEntry('site1.com', 'peach', 11),
        FakeDataMaker.passwordEntry('google.com', 'mario', 7),
        FakeDataMaker.passwordEntry('site2.com', 'luigi', 8),
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertEquals(passwordList, passwordsSection.$.passwordList.items);

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
    });

    // Test verifies that removing a password will update the elements.
    test('verifyPasswordListRemove', function() {
      var passwordList = [
        FakeDataMaker.passwordEntry('anotherwebsite.com', 'luigi', 1),
        FakeDataMaker.passwordEntry('longwebsite.com', 'peach', 7),
        FakeDataMaker.passwordEntry('website.com', 'mario', 70)
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
      // Simulate 'longwebsite.com' being removed from the list.
      passwordsSection.splice('savedPasswords', 1, 1);
      self.flushPasswordSection_(passwordsSection);

      assertFalse(self.listContainsUrl(passwordsSection.savedPasswords,
                                       'longwebsite.com'));
      assertFalse(self.listContainsUrl(passwordList, 'longwebsite.com'));

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          passwordList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any passwords.
    test('verifyPasswordItemRemoveButton', function(done) {
      var passwordList = [
        FakeDataMaker.passwordEntry('one', 'six', 5),
        FakeDataMaker.passwordEntry('two', 'five', 3),
        FakeDataMaker.passwordEntry('three', 'four', 1),
        FakeDataMaker.passwordEntry('four', 'three', 2),
        FakeDataMaker.passwordEntry('five', 'two', 4),
        FakeDataMaker.passwordEntry('six', 'one', 6),
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);

      var passwords =
          self.getIronListChildren_(passwordsSection.$.passwordList);

      // The index of the button currently being checked.
      var index = 0;

      var clickRemoveButton = function() {
        MockInteractions.tap(passwords[index].querySelector('#passwordMenu'));
        MockInteractions.tap(passwordsSection.$.menuRemovePassword);
      };

      // Listen for the remove event. If this event isn't received, the test
      // will time out and fail.
      passwordsSection.addEventListener('remove-saved-password',
                                        function(event) {
        // Verify that the event matches the expected value.
        assertTrue(index < passwordList.length);
        assertEquals(passwordList[index].loginPair.originUrl,
                     event.detail.originUrl);
        assertEquals(passwordList[index].loginPair.username,
                     event.detail.username);

        if (++index < passwordList.length) {
          // Click 'remove' on all passwords, one by one.
          window.setTimeout(clickRemoveButton, 0);
        } else {
          done();
        }
      });

      // Start removing.
      clickRemoveButton();
    });

    test('verifyFilterPasswords', function() {
      var passwordList = [
        FakeDataMaker.passwordEntry('one.com', 'show', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('four.com', 'three', 2),
        FakeDataMaker.passwordEntry('five.com', 'two', 4),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      var passwordsSection = self.createPasswordsSection_(passwordList, []);
      passwordsSection.filter = 'show';
      Polymer.dom.flush();

      var expectedPasswordList = [
        FakeDataMaker.passwordEntry('one.com', 'show', 5),
        FakeDataMaker.passwordEntry('two.com', 'shower', 3),
        FakeDataMaker.passwordEntry('three.com/show', 'four', 1),
        FakeDataMaker.passwordEntry('six-show.com', 'one', 6),
      ];

      self.validatePasswordList(
          self.getIronListChildren_(passwordsSection.$.passwordList),
          expectedPasswordList);
    });

    test('verifyFilterPasswordExceptions', function() {
      var exceptionList = [
        FakeDataMaker.exceptionEntry('docsshow.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);
      passwordsSection.filter = 'show';
      Polymer.dom.flush();

      var expectedExceptionList = [
        FakeDataMaker.exceptionEntry('docsshow.google.com'),
        FakeDataMaker.exceptionEntry('showmail.com'),
        FakeDataMaker.exceptionEntry('mapsshow.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.comshow'),
      ];

      self.validateExceptionList_(
          self.getDomRepeatChildren_(passwordsSection.$.passwordExceptionsList),
          expectedExceptionList);
    });

    test('verifyPasswordExceptions', function() {
      var exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      self.validateExceptionList_(
          self.getDomRepeatChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that removing an exception will update the elements.
    test('verifyPasswordExceptionRemove', function() {
      var exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      self.validateExceptionList_(
          self.getDomRepeatChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      // Simulate 'mail.com' being removed from the list.
      passwordsSection.splice('passwordExceptions', 1, 1);
      assertFalse(self.exceptionsListContainsUrl(
            passwordsSection.passwordExceptions, 'mail.com'));
      assertFalse(self.exceptionsListContainsUrl(exceptionList, 'mail.com'));
      self.flushPasswordSection_(passwordsSection);

      self.validateExceptionList_(
          self.getDomRepeatChildren_(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      var exceptionList = [
        FakeDataMaker.exceptionEntry('docs.google.com'),
        FakeDataMaker.exceptionEntry('mail.com'),
        FakeDataMaker.exceptionEntry('google.com'),
        FakeDataMaker.exceptionEntry('inbox.google.com'),
        FakeDataMaker.exceptionEntry('maps.google.com'),
        FakeDataMaker.exceptionEntry('plus.google.com'),
      ];

      var passwordsSection = self.createPasswordsSection_([], exceptionList);

      var exceptions =
          self.getDomRepeatChildren_(passwordsSection.$.passwordExceptionsList);

      // The index of the button currently being checked.
      var index = 0;

      var clickRemoveButton = function() {
        MockInteractions.tap(
            exceptions[index].querySelector('#removeExceptionButton'));
      };

      // Listen for the remove event. If this event isn't received, the test
      // will time out and fail.
      passwordsSection.addEventListener('remove-password-exception',
                                        function(event) {
        // Verify that the event matches the expected value.
        assertTrue(index < exceptionList.length);
        assertEquals(exceptionList[index].exceptionUrl, event.detail);

        if (++index < exceptionList.length)
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        else
          done();
      });

      // Start removing.
      clickRemoveButton();
    });

    test('showSavedPassword', function() {
      var PASSWORD = 'bAn@n@5';
      var item = FakeDataMaker.passwordEntry('goo.gl', 'bart', PASSWORD.length);
      var passwordDialog = self.createPasswordDialog_(item);

      passwordDialog.password = PASSWORD;
      passwordDialog.showPassword = true;

      Polymer.dom.flush();

      assertEquals(PASSWORD,
                   passwordDialog.$.passwordInput.value);
      // Password should be visible.
      assertEquals('text',
                   passwordDialog.$.passwordInput.type);
    });

    // Test will timeout if event is not received.
    test('onShowSavedPassword', function(done) {
      var item = FakeDataMaker.passwordEntry('goo.gl', 'bart', 1);
      var passwordDialog = self.createPasswordDialog_(item);

      passwordDialog.addEventListener('show-password', function(event) {
        assertEquals(item.loginPair.originUrl, event.detail.originUrl);
        assertEquals(item.loginPair.username, event.detail.username);
        done();
      });

      MockInteractions.tap(passwordDialog.$.showPasswordButton);
    });
  });

  mocha.run();
});
