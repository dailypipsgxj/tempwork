// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_H_
#define CRYPTO_NSS_UTIL_H_

#include <stdint.h>

#include <string>
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "crypto/crypto_export.h"

namespace base {
class FilePath;
class Lock;
class Time;
}  // namespace base

// This file specifically doesn't depend on any NSS or NSPR headers because it
// is included by various (non-crypto) parts of chrome to call the
// initialization functions.
namespace crypto {

// EarlySetupForNSSInit performs lightweight setup which must occur before the
// process goes multithreaded. This does not initialise NSS. For test, see
// EnsureNSSInit.
CRYPTO_EXPORT void EarlySetupForNSSInit();

// Initialize NRPR if it isn't already initialized.  This function is
// thread-safe, and NSPR will only ever be initialized once.
CRYPTO_EXPORT void EnsureNSPRInit();

// Initialize NSS if it isn't already initialized.  This must be called before
// any other NSS functions.  This function is thread-safe, and NSS will only
// ever be initialized once.
CRYPTO_EXPORT void EnsureNSSInit();

// Check if the current NSS version is greater than or equals to |version|.
// A sample version string is "3.12.3".
bool CheckNSSVersion(const char* version);

#if defined(OS_CHROMEOS)
// Indicates that NSS should use the Chaps library so that we
// can access the TPM through NSS.  InitializeTPMTokenAndSystemSlot and
// InitializeTPMForChromeOSUser must still be called to load the slots.
CRYPTO_EXPORT void EnableTPMTokenForNSS();

// Returns true if EnableTPMTokenForNSS has been called.
CRYPTO_EXPORT bool IsTPMTokenEnabledForNSS();

// Returns true if the TPM is owned and PKCS#11 initialized with the
// user and security officer PINs, and has been enabled in NSS by
// calling EnableTPMForNSS, and Chaps has been successfully
// loaded into NSS.
// If |callback| is non-null and the function returns false, the |callback| will
// be run once the TPM is ready. |callback| will never be run if the function
// returns true.
CRYPTO_EXPORT bool IsTPMTokenReady(const base::Closure& callback)
    WARN_UNUSED_RESULT;

// Initialize the TPM token and system slot. The |callback| will run on the same
// thread with true if the token and slot were successfully loaded or were
// already initialized. |callback| will be passed false if loading failed.  Once
// called, InitializeTPMTokenAndSystemSlot must not be called again until the
// |callback| has been run.
CRYPTO_EXPORT void InitializeTPMTokenAndSystemSlot(
    int system_slot_id,
    const base::Callback<void(bool)>& callback);
#endif

// Convert a NSS PRTime value into a base::Time object.
// We use a int64_t instead of PRTime here to avoid depending on NSPR headers.
CRYPTO_EXPORT base::Time PRTimeToBaseTime(int64_t prtime);

// Convert a base::Time object into a PRTime value.
// We use a int64_t instead of PRTime here to avoid depending on NSPR headers.
CRYPTO_EXPORT int64_t BaseTimeToPRTime(base::Time time);

// NSS has a bug which can cause a deadlock or stall in some cases when writing
// to the certDB and keyDB. It also has a bug which causes concurrent key pair
// generations to scribble over each other. To work around this, we synchronize
// writes to the NSS databases with a global lock. The lock is hidden beneath a
// function for easy disabling when the bug is fixed. Callers should allow for
// it to return NULL in the future.
//
// See https://bugzilla.mozilla.org/show_bug.cgi?id=564011
base::Lock* GetNSSWriteLock();

// A helper class that acquires the NSS write Lock while the AutoNSSWriteLock
// is in scope.
class CRYPTO_EXPORT AutoNSSWriteLock {
 public:
  AutoNSSWriteLock();
  ~AutoNSSWriteLock();
 private:
  base::Lock *lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoNSSWriteLock);
};

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_H_
