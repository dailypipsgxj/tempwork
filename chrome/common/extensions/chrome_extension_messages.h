// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-specific IPC messages for extensions.
// Extension-related messages that aren't specific to Chrome live in
// extensions/common/extension_messages.h.
//
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "chrome/common/extensions/api/webstore/webstore_api_constants.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/common/stack_frame.h"
#include "ipc/ipc_message_macros.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START ChromeExtensionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(extensions::api::webstore::InstallStage,
                          extensions::api::webstore::INSTALL_STAGE_INSTALLING)
IPC_ENUM_TRAITS_MAX_VALUE(extensions::webstore_install::Result,
                          extensions::webstore_install::RESULT_LAST)

// Messages sent from the browser to the renderer.

// Toggles visual muting of the render view area. This is on when a constrained
// window is showing.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetVisuallyDeemphasized,
                    bool /* deemphazied */)

// Sent to the renderer if install stage updates were requested for an inline
// install.
IPC_MESSAGE_ROUTED1(ExtensionMsg_InlineInstallStageChanged,
                    extensions::api::webstore::InstallStage /* stage */)

// Sent to the renderer if download progress updates were requested for an
// inline install.
IPC_MESSAGE_ROUTED1(ExtensionMsg_InlineInstallDownloadProgress,
                    int /* percent_downloaded */)

// Send to renderer once the installation mentioned on
// ExtensionHostMsg_InlineWebstoreInstall is complete.
IPC_MESSAGE_ROUTED4(ExtensionMsg_InlineWebstoreInstallResponse,
                    int32_t /* install id */,
                    bool /* whether the install was successful */,
                    std::string /* error */,
                    extensions::webstore_install::Result /* result */)

IPC_STRUCT_TRAITS_BEGIN(ui::AXNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(transform)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::AXTreeData)
  IPC_STRUCT_TRAITS_MEMBER(tree_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(focused_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(mimetype)
  IPC_STRUCT_TRAITS_MEMBER(doctype)
  IPC_STRUCT_TRAITS_MEMBER(loaded)
  IPC_STRUCT_TRAITS_MEMBER(loading_progress)
  IPC_STRUCT_TRAITS_MEMBER(focus_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_affinity)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_affinity)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::AXTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(has_tree_data)
  IPC_STRUCT_TRAITS_MEMBER(tree_data)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(root_id)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ExtensionMsg_AccessibilityEventParams)
  // ID of the accessibility tree that this event applies to.
  IPC_STRUCT_MEMBER(int, tree_id)

  // The global offset of all coordinates in this accessibility tree.
  IPC_STRUCT_MEMBER(gfx::Vector2d, location_offset)

  // The tree update.
  IPC_STRUCT_MEMBER(ui::AXTreeUpdate, update)

  // Type of event.
  IPC_STRUCT_MEMBER(ui::AXEvent, event_type)

  // ID of the node that the event applies to.
  IPC_STRUCT_MEMBER(int, id)
IPC_STRUCT_END()

// Forward an accessibility message to an extension process where an
// extension is using the automation API to listen for accessibility events.
IPC_MESSAGE_ROUTED2(ExtensionMsg_AccessibilityEvent,
                    ExtensionMsg_AccessibilityEventParams,
                    bool /* is_active_profile */)

// Messages sent from the renderer to the browser.


// Sent by the renderer to implement chrome.webstore.install().
IPC_MESSAGE_ROUTED5(ExtensionHostMsg_InlineWebstoreInstall,
                    int32_t /* install id */,
                    int32_t /* return route id */,
                    std::string /* Web Store item ID */,
                    GURL /* requestor URL */,
                    int /* listeners_mask */)
