// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_switches.h"

namespace spellcheck {
namespace switches {

#if defined(ENABLE_SPELLCHECK)
#if defined(OS_ANDROID)
// Enables use of the Android spellchecker.
const char kEnableAndroidSpellChecker[] = "enable-android-spellchecker";
#endif  // defined(OS_ANDROID)

// Enables participation in the field trial for user feedback to spelling
// service.
const char kEnableSpellingFeedbackFieldTrial[] =
    "enable-spelling-feedback-field-trial";

// Specifies the number of seconds between sending batches of feedback to
// spelling service. The default is 30 minutes. The minimum is 5 seconds. This
// switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackIntervalSeconds[] =
    "spelling-service-feedback-interval-seconds";

// Specifies the URL where spelling service feedback data will be sent instead
// of the default URL. This switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackUrl[] = "spelling-service-feedback-url";
#endif  // defined(ENABLE_SPELLCHECK)

}  // namespace switches
}  // namespace spellcheck
