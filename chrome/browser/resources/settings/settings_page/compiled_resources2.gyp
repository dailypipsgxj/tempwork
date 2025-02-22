# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main_page_behavior',
      'dependencies': [
        'settings_section',
        'transition_behavior',
        '<(EXTERNS_GYP):settings_private',
        '<(EXTERNS_GYP):web_animations',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_animated_pages',
      'dependencies': [
        '../compiled_resources2.gyp:route',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_page_visibility',
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_router',
      'dependencies': [
        '../compiled_resources2.gyp:route',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_section',
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_subpage',
      'dependencies': [
        'settings_subpage_search',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-resizable-behavior/compiled_resources2.gyp:iron-resizable-behavior-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/neon-animation/compiled_resources2.gyp:neon-animatable-behavior-extracted',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_subpage_search',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-icon-button/compiled_resources2.gyp:paper-icon-button-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-input/compiled_resources2.gyp:paper-input-container-extracted',
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_search_field/compiled_resources2.gyp:cr_search_field_behavior',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'transition_behavior',
      'dependencies': [
        '<(EXTERNS_GYP):settings_private',
        '<(EXTERNS_GYP):web_animations',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
