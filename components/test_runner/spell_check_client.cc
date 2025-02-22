// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/spell_check_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

namespace test_runner {

SpellCheckClient::SpellCheckClient(TestRunner* test_runner)
    : last_requested_text_checking_completion_(nullptr),
      test_runner_(test_runner),
      weak_factory_(this) {
  DCHECK(test_runner);
}

SpellCheckClient::~SpellCheckClient() {
}

void SpellCheckClient::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

// blink::WebSpellCheckClient
void SpellCheckClient::spellCheck(
    const blink::WebString& text,
    int& misspelled_offset,
    int& misspelled_length,
    blink::WebVector<blink::WebString>* optional_suggestions) {
  // Check the spelling of the given text.
  spell_check_.SpellCheckWord(text, &misspelled_offset, &misspelled_length);
}

void SpellCheckClient::checkTextOfParagraph(
    const blink::WebString& text,
    blink::WebTextCheckingTypeMask mask,
    blink::WebVector<blink::WebTextCheckingResult>* web_results) {
  std::vector<blink::WebTextCheckingResult> results;
  if (mask & blink::WebTextCheckingTypeSpelling) {
    size_t offset = 0;
    base::string16 data = text;
    while (offset < data.length()) {
      int misspelled_position = 0;
      int misspelled_length = 0;
      spell_check_.SpellCheckWord(
          data.substr(offset), &misspelled_position, &misspelled_length);
      if (!misspelled_length)
        break;
      blink::WebTextCheckingResult result;
      result.decoration = blink::WebTextDecorationTypeSpelling;
      result.location = offset + misspelled_position;
      result.length = misspelled_length;
      results.push_back(result);
      offset += misspelled_position + misspelled_length;
    }
  }
  web_results->assign(results);
}

void SpellCheckClient::requestCheckingOfText(
    const blink::WebString& text,
    const blink::WebVector<uint32_t>& markers,
    const blink::WebVector<unsigned>& marker_offsets,
    blink::WebTextCheckingCompletion* completion) {
  if (text.isEmpty()) {
    if (completion)
      completion->didCancelCheckingText();
    return;
  }

  if (last_requested_text_checking_completion_)
    last_requested_text_checking_completion_->didCancelCheckingText();

  last_requested_text_checking_completion_ = completion;
  last_requested_text_check_string_ = text;
  if (spell_check_.HasInCache(text))
    FinishLastTextCheck();
  else
    delegate_->PostDelayedTask(
        new WebCallbackTask(base::Bind(&SpellCheckClient::FinishLastTextCheck,
                                       weak_factory_.GetWeakPtr())),
        0);
}

void SpellCheckClient::FinishLastTextCheck() {
  if (!last_requested_text_checking_completion_)
    return;
  std::vector<blink::WebTextCheckingResult> results;
  int offset = 0;
  base::string16 text = last_requested_text_check_string_;
  if (!spell_check_.IsMultiWordMisspelling(blink::WebString(text), &results)) {
    while (text.length()) {
      int misspelled_position = 0;
      int misspelled_length = 0;
      spell_check_.SpellCheckWord(
          blink::WebString(text), &misspelled_position, &misspelled_length);
      if (!misspelled_length)
        break;
      blink::WebVector<blink::WebString> suggestions;
      spell_check_.FillSuggestionList(
          blink::WebString(text.substr(misspelled_position, misspelled_length)),
          &suggestions);
      results.push_back(blink::WebTextCheckingResult(
          blink::WebTextDecorationTypeSpelling,
          offset + misspelled_position,
          misspelled_length,
          suggestions.isEmpty() ? blink::WebString() : suggestions[0]));
      text = text.substr(misspelled_position + misspelled_length);
      offset += misspelled_position + misspelled_length;
    }
  }
  last_requested_text_checking_completion_->didFinishCheckingText(results);
  last_requested_text_checking_completion_ = 0;

  if (test_runner_->shouldDumpSpellCheckCallbacks())
    delegate_->PrintMessage("SpellCheckEvent: FinishLastTextCheck\n");
}

}  // namespace test_runner
