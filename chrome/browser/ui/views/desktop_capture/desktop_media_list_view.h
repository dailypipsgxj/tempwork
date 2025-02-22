// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_

#include "chrome/browser/media/desktop_media_list_observer.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/views/view.h"

class DesktopMediaPickerDialogView;

// View that shows a list of desktop media sources available from
// DesktopMediaList.
class DesktopMediaListView : public views::View,
                             public DesktopMediaListObserver {
 public:
  DesktopMediaListView(DesktopMediaPickerDialogView* parent,
                       std::unique_ptr<DesktopMediaList> media_list,
                       DesktopMediaSourceViewStyle generic_style,
                       DesktopMediaSourceViewStyle single_style,
                       const base::string16& accessible_name);

  ~DesktopMediaListView() override;

  void StartUpdating(content::DesktopMediaID dialog_window_id);

  // Called by DesktopMediaSourceView when selection has changed.
  void OnSelectionChanged();

  // Called by DesktopMediaSourceView when a source has been double-clicked.
  void OnDoubleClick();

  // Returns currently selected source.
  DesktopMediaSourceView* GetSelection();

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;

 private:
  // DesktopMediaList::Observer interface
  void OnSourceAdded(DesktopMediaList* list, int index) override;
  void OnSourceRemoved(DesktopMediaList* list, int index) override;
  void OnSourceMoved(DesktopMediaList* list,
                     int old_index,
                     int new_index) override;
  void OnSourceNameChanged(DesktopMediaList* list, int index) override;
  void OnSourceThumbnailChanged(DesktopMediaList* list, int index) override;

  // Accepts whatever happens to be selected right now.
  void AcceptSelection();

  // Change the source style of this list on the fly.
  void SetStyle(DesktopMediaSourceViewStyle* style);

  DesktopMediaPickerDialogView* parent_;
  std::unique_ptr<DesktopMediaList> media_list_;

  DesktopMediaSourceViewStyle single_style_;
  DesktopMediaSourceViewStyle generic_style_;
  DesktopMediaSourceViewStyle* active_style_;

  base::string16 accessible_name_;

  base::WeakPtrFactory<DesktopMediaListView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
