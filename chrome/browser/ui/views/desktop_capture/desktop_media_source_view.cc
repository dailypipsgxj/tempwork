// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"

#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

using content::DesktopMediaID;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    const DesktopMediaSourceViewStyle& style) = default;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    int columns,
    const gfx::Size& item_size,
    const gfx::Rect& label_rect,
    gfx::HorizontalAlignment text_alignment,
    const gfx::Rect& image_rect,
    int selection_border_thickness,
    int focus_rectangle_inset)
    : columns(columns),
      item_size(item_size),
      label_rect(label_rect),
      text_alignment(text_alignment),
      image_rect(image_rect),
      selection_border_thickness(selection_border_thickness),
      focus_rectangle_inset(focus_rectangle_inset) {}

DesktopMediaSourceView::DesktopMediaSourceView(
    DesktopMediaListView* parent,
    DesktopMediaID source_id,
    DesktopMediaSourceViewStyle style)
    : parent_(parent),
      source_id_(source_id),
      style_(style),
      image_view_(new views::ImageView()),
      label_(new views::Label()),
      selected_(false) {
  AddChildView(image_view_);
  AddChildView(label_);
  image_view_->set_interactive(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetStyle(style_);
}

DesktopMediaSourceView::~DesktopMediaSourceView() {}

const char* DesktopMediaSourceView::kDesktopMediaSourceViewClassName =
    "DesktopMediaPicker_DesktopMediaSourceView";

void DesktopMediaSourceView::SetName(const base::string16& name) {
  label_->SetText(name);
}

void DesktopMediaSourceView::SetThumbnail(const gfx::ImageSkia& thumbnail) {
  image_view_->SetImage(thumbnail);
}

void DesktopMediaSourceView::SetSelected(bool selected) {
  if (selected == selected_)
    return;
  selected_ = selected;

  if (selected) {
    // Unselect all other sources.
    Views neighbours;
    parent()->GetViewsInGroup(GetGroup(), &neighbours);
    for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
      if (*i != this) {
        DCHECK_EQ((*i)->GetClassName(),
                  DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
        DesktopMediaSourceView* source_view =
            static_cast<DesktopMediaSourceView*>(*i);
        source_view->SetSelected(false);
      }
    }

    const SkColor border_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
    image_view_->SetBorder(views::Border::CreateSolidBorder(
        style_.selection_border_thickness, border_color));
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::BOLD));
    parent_->OnSelectionChanged();
  } else {
    image_view_->SetBorder(views::Border::NullBorder());
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::NORMAL));
  }

  SchedulePaint();
}

void DesktopMediaSourceView::SetHovered(bool hovered) {
  if (hovered) {
    // Use background color to show mouse hover.
    const SkColor bg_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_HoverMenuItemBackgroundColor);
    set_background(views::Background::CreateSolidBackground(bg_color));
  } else {
    set_background(nullptr);
  }

  SchedulePaint();
}

const char* DesktopMediaSourceView::GetClassName() const {
  return DesktopMediaSourceView::kDesktopMediaSourceViewClassName;
}

void DesktopMediaSourceView::SetStyle(DesktopMediaSourceViewStyle style) {
  style_ = style;
  image_view_->SetBoundsRect(style_.image_rect);
  if (selected_) {
    const SkColor border_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
    image_view_->SetBorder(views::Border::CreateSolidBorder(
        style_.selection_border_thickness, border_color));
  }
  label_->SetBoundsRect(style_.label_rect);
  label_->SetHorizontalAlignment(style_.text_alignment);
}

views::View* DesktopMediaSourceView::GetSelectedViewForGroup(int group) {
  Views neighbours;
  parent()->GetViewsInGroup(group, &neighbours);
  if (neighbours.empty())
    return nullptr;

  for (Views::iterator i(neighbours.begin()); i != neighbours.end(); ++i) {
    DCHECK_EQ((*i)->GetClassName(),
              DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(*i);
    if (source_view->selected_)
      return source_view;
  }
  return nullptr;
}

bool DesktopMediaSourceView::IsGroupFocusTraversable() const {
  return false;
}

void DesktopMediaSourceView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus()) {
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(style_.focus_rectangle_inset, style_.focus_rectangle_inset);
    canvas->DrawFocusRect(bounds);
  }
}

void DesktopMediaSourceView::OnFocus() {
  View::OnFocus();
  SetSelected(true);
  ScrollRectToVisible(gfx::Rect(size()));
  // We paint differently when focused.
  SchedulePaint();
}

void DesktopMediaSourceView::OnBlur() {
  View::OnBlur();
  // We paint differently when focused.
  SchedulePaint();
}

bool DesktopMediaSourceView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.GetClickCount() == 1) {
    RequestFocus();
  } else if (event.GetClickCount() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
  }
  return true;
}

void DesktopMediaSourceView::OnMouseEntered(const ui::MouseEvent& event) {
  SetHovered(true);
}

void DesktopMediaSourceView::OnMouseExited(const ui::MouseEvent& event) {
  SetHovered(false);
}

void DesktopMediaSourceView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      event->details().tap_count() == 2) {
    RequestFocus();
    parent_->OnDoubleClick();
    event->SetHandled();
    return;
  }

  // Detect tap gesture using ET_GESTURE_TAP_DOWN so the view also gets focused
  // on the long tap (when the tap gesture starts).
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocus();
    event->SetHandled();
  }
}

void DesktopMediaSourceView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = label_->text();
}
