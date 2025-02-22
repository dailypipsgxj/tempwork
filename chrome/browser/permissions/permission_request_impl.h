// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/permission_type.h"

class GURL;
class PermissionContextBase;
class Profile;

// Default implementation of PermissionRequest, it is assumed that the
// caller owns it and that it can be deleted once the |delete_callback|
// is executed.
class PermissionRequestImpl : public PermissionRequest {
 public:
  using PermissionDecidedCallback = base::Callback<void(bool, ContentSetting)>;

  PermissionRequestImpl(
      const GURL& request_origin,
      content::PermissionType permission_type,
      Profile* profile,
      bool has_gesture,
      const PermissionDecidedCallback& permission_decided_callback,
      const base::Closure delete_callback);

  ~PermissionRequestImpl() override;

 protected:
  void RegisterActionTaken() { action_taken_ = true; }

 private:
  // PermissionRequest:
  gfx::VectorIconId GetVectorIconId() const override;
  int GetIconId() const override;
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;
  // Remember to call RegisterActionTaken for these methods if you are
  // overriding them.
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestType GetPermissionRequestType() const override;
  PermissionRequestGestureType GetGestureType() const override;

  GURL request_origin_;
  content::PermissionType permission_type_;
  Profile* profile_;
  bool has_gesture_;

  // Called once a decision is made about the permission.
  const PermissionDecidedCallback permission_decided_callback_;

  // Called when the request is no longer in use so it can be deleted by the
  // caller.
  const base::Closure delete_callback_;
  bool is_finished_;
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestImpl);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
