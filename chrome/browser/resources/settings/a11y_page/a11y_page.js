// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the settings page containing accessibility
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-a11y-page prefs="{{prefs}}"></settings-a11y-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-a11y-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

<if expr="chromeos">
    autoClickDelayOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        return [
          {value: 600,
           name: loadTimeData.getString('delayBeforeClickExtremelyShort')},
          {value: 800,
           name: loadTimeData.getString('delayBeforeClickVeryShort')},
          {value: 1000,
           name: loadTimeData.getString('delayBeforeClickShort')},
          {value: 2000,
           name: loadTimeData.getString('delayBeforeClickLong')},
          {value: 4000,
           name: loadTimeData.getString('delayBeforeClickVeryLong')},
        ];
      },
    },

    /**
     * Whether to show experimental accessibility features.
     * @private {boolean}
     */
    showExperimentalFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showExperimentalA11yFeatures');
      },
    }
</if>
  },

  /** @private */
  onMoreFeaturesTap_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },
});
