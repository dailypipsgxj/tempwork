// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_enterprise_reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/user_data/arc_user_data_service.h"

namespace arc {

ArcEnterpriseReportingService::ArcEnterpriseReportingService(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_ptr_factory_(this) {
  arc_bridge_service()->enterprise_reporting()->AddObserver(this);
}

ArcEnterpriseReportingService::~ArcEnterpriseReportingService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->enterprise_reporting()->RemoveObserver(this);
}

void ArcEnterpriseReportingService::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->enterprise_reporting()->instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcEnterpriseReportingService::ReportManagementState(
    mojom::ManagementState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "ReportManagementState state=" << state;

  if (state == mojom::ManagementState::MANAGED_DO_LOST) {
    DCHECK(arc::ArcServiceManager::Get());
    ArcServiceManager::Get()->arc_user_data_service()->RequireUserDataWiped(
        base::Bind(&ArcEnterpriseReportingService::RestartArc,
                   weak_ptr_factory_.GetWeakPtr()));
    ArcAuthService::Get()->StopArc();
  }
}

void ArcEnterpriseReportingService::RestartArc(bool result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!result)
    LOG(ERROR) << "Required ARC user data wipe failed.";

  // Restart ARC anyway. Let the enterprise reporting instance decide whether
  // the ARC user data wipe is still required or not.
  VLOG(1) << "Restart ARC";
  ArcAuthService::Get()->EnableArc();
}

}  // namespace arc
