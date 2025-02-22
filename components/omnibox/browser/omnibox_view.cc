// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "components/omnibox/browser/omnibox_view.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/toolbar/toolbar_model.h"
#include "grit/components_scaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

// static
base::string16 OmniboxView::StripJavascriptSchemas(const base::string16& text) {
  const base::string16 kJsPrefix(
      base::ASCIIToUTF16(url::kJavaScriptScheme) + base::ASCIIToUTF16(":"));
  base::string16 out(text);
  while (base::StartsWith(out, kJsPrefix,
                          base::CompareCase::INSENSITIVE_ASCII)) {
    base::TrimWhitespace(out.substr(kJsPrefix.length()), base::TRIM_LEADING,
                         &out);
  }
  return out;
}

// static
base::string16 OmniboxView::SanitizeTextForPaste(const base::string16& text) {
  // Check for non-newline whitespace; if found, collapse whitespace runs down
  // to single spaces.
  // TODO(shess): It may also make sense to ignore leading or
  // trailing whitespace when making this determination.
  for (size_t i = 0; i < text.size(); ++i) {
    if (base::IsUnicodeWhitespace(text[i]) &&
        text[i] != '\n' && text[i] != '\r') {
      const base::string16 collapsed = base::CollapseWhitespace(text, false);
      // If the user is pasting all-whitespace, paste a single space
      // rather than nothing, since pasting nothing feels broken.
      return collapsed.empty() ?
          base::ASCIIToUTF16(" ") : StripJavascriptSchemas(collapsed);
    }
  }

  // Otherwise, all whitespace is newlines; remove it entirely.
  return StripJavascriptSchemas(base::CollapseWhitespace(text, true));
}

OmniboxView::~OmniboxView() {
}

void OmniboxView::OpenMatch(const AutocompleteMatch& match,
                            WindowOpenDisposition disposition,
                            const GURL& alternate_nav_url,
                            const base::string16& pasted_text,
                            size_t selected_line) {
  // Invalid URLs such as chrome://history can end up here.
  if (!match.destination_url.is_valid() || !model_)
    return;
  const AutocompleteMatch::Type match_type = match.type;
  model_->OpenMatch(
      match, disposition, alternate_nav_url, pasted_text, selected_line);
  // WARNING: |match| may refer to a deleted object at this point!
  OnMatchOpened(match_type);
}

bool OmniboxView::IsEditingOrEmpty() const {
  return (model_.get() && model_->user_input_in_progress()) ||
      (GetOmniboxTextLength() == 0);
}

int OmniboxView::GetIcon() const {
  if (!IsEditingOrEmpty())
    return controller_->GetToolbarModel()->GetIcon();
  int id = AutocompleteMatch::TypeToIcon(model_.get() ?
      model_->CurrentTextType() : AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  return (id == IDR_OMNIBOX_HTTP) ? IDR_LOCATION_BAR_HTTP : id;
}

gfx::VectorIconId OmniboxView::GetVectorIcon() const {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (!IsEditingOrEmpty())
    return controller_->GetToolbarModel()->GetVectorIcon();

  return AutocompleteMatch::TypeToVectorIcon(
      model_ ? model_->CurrentTextType()
             : AutocompleteMatchType::URL_WHAT_YOU_TYPED);
#else
  NOTIMPLEMENTED();
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#endif
}

void OmniboxView::SetUserText(const base::string16& text) {
  SetUserText(text, true);
}

void OmniboxView::SetUserText(const base::string16& text,
                              bool update_popup) {
  if (model_.get())
    model_->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxView::ShowURL() {
  SetFocus();
  controller_->GetToolbarModel()->set_url_replacement_enabled(false);
  model_->UpdatePermanentText();
  RevertWithoutResettingSearchTermReplacement();
  SelectAll(true);
}

void OmniboxView::HideURL() {
  controller_->GetToolbarModel()->set_url_replacement_enabled(true);
  model_->UpdatePermanentText();
  RevertWithoutResettingSearchTermReplacement();
}

void OmniboxView::RevertAll() {
  controller_->GetToolbarModel()->set_url_replacement_enabled(true);
  RevertWithoutResettingSearchTermReplacement();
}

void OmniboxView::RevertWithoutResettingSearchTermReplacement() {
  CloseOmniboxPopup();
  if (model_.get())
    model_->Revert();
  TextChanged();
}

void OmniboxView::CloseOmniboxPopup() {
  if (model_.get())
    model_->StopAutocomplete();
}

bool OmniboxView::IsImeShowingPopup() const {
  // Default to claiming that the IME is not showing a popup, since hiding the
  // omnibox dropdown is a bad user experience when we don't know for sure that
  // we have to.
  return false;
}

void OmniboxView::ShowImeIfNeeded() {
}

bool OmniboxView::IsIndicatingQueryRefinement() const {
  // The default implementation always returns false.  Mobile ports can override
  // this method and implement as needed.
  return false;
}

void OmniboxView::OnMatchOpened(AutocompleteMatch::Type match_type) {
}

void OmniboxView::GetState(State* state) {
  state->text = GetText();
  state->keyword = model()->keyword();
  state->is_keyword_selected = model()->is_keyword_selected();
  GetSelectionBounds(&state->sel_start, &state->sel_end);
}

OmniboxView::StateChanges OmniboxView::GetStateChanges(const State& before,
                                                     const State& after) {
  OmniboxView::StateChanges state_changes;
  state_changes.old_text = &before.text;
  state_changes.new_text = &after.text;
  state_changes.new_sel_start = after.sel_start;
  state_changes.new_sel_end = after.sel_end;
  const bool old_sel_empty = before.sel_start == before.sel_end;
  const bool new_sel_empty = after.sel_start == after.sel_end;
  const bool sel_same_ignoring_direction =
      std::min(before.sel_start, before.sel_end) ==
          std::min(after.sel_start, after.sel_end) &&
      std::max(before.sel_start, before.sel_end) ==
          std::max(after.sel_start, after.sel_end);
  state_changes.selection_differs =
      (!old_sel_empty || !new_sel_empty) && !sel_same_ignoring_direction;
  state_changes.text_differs = before.text != after.text;
  state_changes.keyword_differs =
      (after.is_keyword_selected != before.is_keyword_selected) ||
      (after.is_keyword_selected && before.is_keyword_selected &&
       after.keyword != before.keyword);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  state_changes.just_deleted_text =
      (before.text.length() > after.text.length()) &&
      (after.sel_start <= std::min(before.sel_start, before.sel_end));

  return state_changes;
}

OmniboxView::OmniboxView(OmniboxEditController* controller,
                         std::unique_ptr<OmniboxClient> client)
    : controller_(controller) {
  // |client| can be null in tests.
  if (client) {
    model_.reset(new OmniboxEditModel(this, controller, std::move(client)));
  }
}

void OmniboxView::TextChanged() {
  EmphasizeURLComponents();
  if (model_.get())
    model_->OnChanged();
}
