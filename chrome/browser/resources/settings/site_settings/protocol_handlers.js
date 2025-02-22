// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'protocol-handlers' is the polymer element for showing the
 * protocol handlers category under Site Settings.
 */

var MenuActions = {
  SET_DEFAULT: 'SetDefault',
  REMOVE: 'Remove',
};

/**
 * @typedef {{host: string,
 *            protocol: string,
 *            spec: string}}
 */
var HandlerEntry;

/**
 * @typedef {{default_handler: number,
 *            handlers: !Array<!HandlerEntry>,
 *            has_policy_recommendations: boolean,
 *            is_default_handler_set_by_user: boolean,
 *            protocol: string}}
 */
var ProtocolEntry;

Polymer({
  is: 'protocol-handlers',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Represents the state of the main toggle shown for the category.
     */
    categoryEnabled: Boolean,

    /**
     * Array of protocols and their handlers.
     * @type {!Array<!ProtocolEntry>}
     */
    protocols: Array,

    /**
     * The possible menu actions.
     * @type {MenuActions}
     */
    menuActions_: {
      type: Object,
      value: MenuActions,
      readOnly: true,
    },
  },

  ready: function() {
    this.addWebUIListener('setHandlersEnabled',
        this.setHandlersEnabled_.bind(this));
    this.addWebUIListener('setProtocolHandlers',
        this.setProtocolHandlers_.bind(this));
    this.addWebUIListener('setIgnoredProtocolHandlers',
        this.setIgnoredProtocolHandlers_.bind(this));
    this.browserProxy.initializeProtocolHandlerList();
  },

  /**
   * Obtains the description for the main toggle.
   * @param {number} categoryEnabled Whether the main toggle is enabled.
   * @return {string} The description to use.
   * @private
   */
  computeHandlersDescription_: function(categoryEnabled) {
    return this.computeCategoryDesc(
        settings.ContentSettingsTypes.PROTOCOL_HANDLERS, categoryEnabled, true);
  },

  /**
   * Returns whether the given index matches the default handler.
   * @param {number} index The index to evaluate.
   * @param {number} defaultHandler The default handler index.
   * @return {boolean} Whether the item is default.
   * @private
   */
  isDefault_: function(index, defaultHandler) {
    return defaultHandler == index;
  },

  /**
   * Updates the main toggle to set it enabled/disabled.
   * @param {boolean} enabled The state to set.
   * @private
   */
  setHandlersEnabled_: function(enabled) {
    this.categoryEnabled = enabled;
  },

  /**
   * Updates the list of protocol handlers.
   * @param {!Array<!ProtocolEntry>} protocols The new protocol handler list.
   * @private
   */
  setProtocolHandlers_: function(protocols) {
    this.protocols = protocols;
  },

  /**
   * Updates the list of ignored protocol handlers.
   * @param {!Array<!ProtocolEntry>} args The new (ignored) protocol handler
   *     list.
   * @private
   */
  setIgnoredProtocolHandlers_: function(args) {
    // TODO(finnur): Figure this out. Have yet to be able to trigger the C++
    // side to send this.
  },

  /**
   * A handler when the toggle is flipped.
   * @private
   */
  onToggleChange_: function(event) {
    this.browserProxy.setProtocolHandlerDefault(this.categoryEnabled);
  },

  /**
   * A handler when an action is selected in the action menu.
   * @private
   */
  onActionMenuIronActivate_: function(event) {
    var protocol = event.model.item.protocol;
    var url = event.model.item.spec;
    if (event.detail.selected == MenuActions.SET_DEFAULT) {
      this.browserProxy.setProtocolDefault(protocol, url);
    } else if (event.detail.selected == MenuActions.REMOVE) {
      this.browserProxy.removeProtocolHandler(protocol, url);
    }
  },
});
