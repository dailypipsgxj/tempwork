// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-button',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /** @private */
    controlled_: {
      type: Boolean,
      computed: 'computeControlled_(pref)',
      reflectToAttribute: true,
    },
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether the button is disabled.
   * @private
   */
  computeControlled_: function(pref) {
    return this.isPrefPolicyControlled(pref);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-button on-tap="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },
});
