// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_child_frame.h"

#include "build/build_config.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

WebContentsViewChildFrame::WebContentsViewChildFrame(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** delegate_view)
    : web_contents_(web_contents),
    delegate_(delegate) {
  *delegate_view = this;
}

WebContentsViewChildFrame::~WebContentsViewChildFrame() {}

WebContentsView* WebContentsViewChildFrame::GetOuterView() {
  return web_contents_->GetOuterWebContents()->GetView();
}

const WebContentsView* WebContentsViewChildFrame::GetOuterView() const {
  return web_contents_->GetOuterWebContents()->GetView();
}

gfx::NativeView WebContentsViewChildFrame::GetNativeView() const {
  return GetOuterView()->GetNativeView();
}

gfx::NativeView WebContentsViewChildFrame::GetContentNativeView() const {
  return GetOuterView()->GetContentNativeView();
}

gfx::NativeWindow WebContentsViewChildFrame::GetTopLevelNativeWindow() const {
  return GetOuterView()->GetTopLevelNativeWindow();
}

void WebContentsViewChildFrame::GetContainerBounds(gfx::Rect* out) const {
  RenderWidgetHostView* view = web_contents_->GetRenderWidgetHostView();
  if (view)
    *out = view->GetViewBounds();
  else
    *out = gfx::Rect();
}

void WebContentsViewChildFrame::SizeContents(const gfx::Size& size) {
  // The RenderWidgetHostViewChildFrame is responsible for sizing the contents.
}

void WebContentsViewChildFrame::SetInitialFocus() {
  NOTREACHED();
}

gfx::Rect WebContentsViewChildFrame::GetViewBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

void WebContentsViewChildFrame::CreateView(const gfx::Size& initial_size,
                                           gfx::NativeView context) {
  // The WebContentsViewChildFrame does not have a native view.
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForWidget(
    RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  return new RenderWidgetHostViewChildFrame(render_widget_host);
}

RenderWidgetHostViewBase* WebContentsViewChildFrame::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return GetOuterView()->CreateViewForPopupWidget(render_widget_host);
}

void WebContentsViewChildFrame::SetPageTitle(const base::string16& title) {
  // The title is ignored for the WebContentsViewChildFrame.
}

void WebContentsViewChildFrame::RenderViewCreated(RenderViewHost* host) {}

void WebContentsViewChildFrame::RenderViewSwappedIn(RenderViewHost* host) {}

void WebContentsViewChildFrame::SetOverscrollControllerEnabled(bool enabled) {
  // This is managed by the outer view.
}

#if defined(OS_MACOSX)
bool WebContentsViewChildFrame::IsEventTracking() const {
  return false;
}

void WebContentsViewChildFrame::CloseTabAfterEventTracking() {
  NOTREACHED();
}

void WebContentsViewChildFrame::SetAllowOtherViews(bool allow) {
  NOTREACHED();
}

bool WebContentsViewChildFrame::GetAllowOtherViews() const {
  NOTREACHED();
  return false;
}
#endif

void WebContentsViewChildFrame::RestoreFocus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::Focus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::StoreFocus() {
  NOTREACHED();
}

DropData* WebContentsViewChildFrame::GetDropData() const {
  NOTREACHED();
  return nullptr;
}

void WebContentsViewChildFrame::UpdateDragCursor(WebDragOperation operation) {
  NOTREACHED();
}

void WebContentsViewChildFrame::GotFocus() {
  NOTREACHED();
}

void WebContentsViewChildFrame::TakeFocus(bool reverse) {
  // TODO(avallee): http://crbug.com/610819 Advance focus to next element in
  // outer WebContents.
}

void WebContentsViewChildFrame::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  NOTREACHED();
}

void WebContentsViewChildFrame::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  NOTREACHED();
}

}  // namespace content
