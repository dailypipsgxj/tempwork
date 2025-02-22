// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_affiliated_match_helper.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

MockAffiliatedMatchHelper::MockAffiliatedMatchHelper()
    : AffiliatedMatchHelper(nullptr,
                            base::WrapUnique<AffiliationService>(nullptr)) {}

MockAffiliatedMatchHelper::~MockAffiliatedMatchHelper() {}

void MockAffiliatedMatchHelper::ExpectCallToGetAffiliatedAndroidRealms(
    const PasswordStore::FormDigest& expected_observed_form,
    const std::vector<std::string>& results_to_return) {
  EXPECT_CALL(*this, OnGetAffiliatedAndroidRealmsCalled(expected_observed_form))
      .WillOnce(testing::Return(results_to_return));
}

void MockAffiliatedMatchHelper::ExpectCallToGetAffiliatedWebRealms(
    const PasswordStore::FormDigest& expected_android_form,
    const std::vector<std::string>& results_to_return) {
  EXPECT_CALL(*this, OnGetAffiliatedWebRealmsCalled(expected_android_form))
      .WillOnce(testing::Return(results_to_return));
}

void MockAffiliatedMatchHelper::ExpectCallToInjectAffiliatedWebRealms(
    const std::vector<std::string>& results_to_inject) {
  EXPECT_CALL(*this, OnInjectAffiliatedWebRealmsCalled())
      .WillOnce(testing::Return(results_to_inject));
}

void MockAffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const PasswordStore::FormDigest& observed_form,
    const AffiliatedRealmsCallback& result_callback) {
  std::vector<std::string> affiliated_android_realms =
      OnGetAffiliatedAndroidRealmsCalled(observed_form);
  result_callback.Run(affiliated_android_realms);
}

void MockAffiliatedMatchHelper::GetAffiliatedWebRealms(
    const PasswordStore::FormDigest& android_form,
    const AffiliatedRealmsCallback& result_callback) {
  std::vector<std::string> affiliated_web_realms =
      OnGetAffiliatedWebRealmsCalled(android_form);
  result_callback.Run(affiliated_web_realms);
}

void MockAffiliatedMatchHelper::InjectAffiliatedWebRealms(
    ScopedVector<autofill::PasswordForm> forms,
    const PasswordFormsCallback& result_callback) {
  std::vector<std::string> affiliated_web_realms =
      OnInjectAffiliatedWebRealmsCalled();
  DCHECK_EQ(affiliated_web_realms.size(), forms.size());
  for (size_t i = 0; i < forms.size(); ++i)
    forms[i]->affiliated_web_realm = affiliated_web_realms[i];
  result_callback.Run(std::move(forms));
}

}  // namespace password_manager
