 /* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"

#include <cert.h>
#include <certdb.h>
#include <pk11pub.h>
#include <secerr.h>

#include "base/logging.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

#if !defined(CERTDB_TERMINAL_RECORD)
/* NSS 3.13 renames CERTDB_VALID_PEER to CERTDB_TERMINAL_RECORD
 * and marks CERTDB_VALID_PEER as deprecated.
 * If we're using an older version, rename it ourselves.
 */
#define CERTDB_TERMINAL_RECORD CERTDB_VALID_PEER
#endif

namespace mozilla_security_manager {

// Based on nsNSSCertificateDB::handleCACertDownload, minus the UI bits.
bool ImportCACerts(PK11SlotInfo* slot,
                   const net::CertificateList& certificates,
                   net::X509Certificate* root,
                   net::NSSCertDatabase::TrustBits trustBits,
                   net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  if (!slot || certificates.empty() || !root)
    return false;

  // Mozilla had some code here to check if a perm version of the cert exists
  // already and use that, but CERT_NewTempCertificate actually does that
  // itself, so we skip it here.

  if (!CERT_IsCACert(root->os_cert_handle(), NULL)) {
    not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
        root, net::ERR_IMPORT_CA_CERT_NOT_CA));
  } else if (root->os_cert_handle()->isperm) {
    // Mozilla just returns here, but we continue in case there are other certs
    // in the list which aren't already imported.
    // TODO(mattm): should we set/add trust if it differs from the present
    // settings?
    not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
        root, net::ERR_IMPORT_CERT_ALREADY_EXISTS));
  } else {
    // Mozilla uses CERT_AddTempCertToPerm, however it is privately exported,
    // and it doesn't take the slot as an argument either.  Instead, we use
    // PK11_ImportCert and CERT_ChangeCertTrust.
    SECStatus srv = PK11_ImportCert(
        slot,
        root->os_cert_handle(),
        CK_INVALID_HANDLE,
        net::x509_util::GetUniqueNicknameForSlot(
            root->GetDefaultNickname(net::CA_CERT),
            &root->os_cert_handle()->derSubject,
            slot).c_str(),
        PR_FALSE /* includeTrust (unused) */);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      return false;
    }
    if (!SetCertTrust(root, net::CA_CERT, trustBits))
      return false;
  }

  PRTime now = PR_Now();
  // Import additional delivered certificates that can be verified.
  // This is sort of merged in from Mozilla's ImportValidCACertsInList.  Mozilla
  // uses CERT_FilterCertListByUsage to filter out non-ca certs, but we want to
  // keep using X509Certificates, so that we can use them to build the
  // |not_imported| result.  So, we keep using our net::CertificateList and
  // filter it ourself.
  for (size_t i = 0; i < certificates.size(); i++) {
    const scoped_refptr<net::X509Certificate>& cert = certificates[i];
    if (cert == root) {
      // we already processed that one
      continue;
    }

    // Mozilla uses CERT_FilterCertListByUsage(certList, certUsageAnyCA,
    // PR_TRUE).  Afaict, checking !CERT_IsCACert on each cert is equivalent.
    if (!CERT_IsCACert(cert->os_cert_handle(), NULL)) {
      not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CA_CERT_NOT_CA));
      VLOG(1) << "skipping cert (non-ca)";
      continue;
    }

    if (cert->os_cert_handle()->isperm) {
      not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CERT_ALREADY_EXISTS));
      VLOG(1) << "skipping cert (perm)";
      continue;
    }

    if (CERT_VerifyCert(CERT_GetDefaultCertDB(), cert->os_cert_handle(),
        PR_TRUE, certUsageVerifyCA, now, NULL, NULL) != SECSuccess) {
      // TODO(mattm): use better error code (map PORT_GetError to an appropriate
      // error value).  (maybe make MapSecurityError or MapCertErrorToCertStatus
      // public.)
      not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
          cert, net::ERR_FAILED));
      VLOG(1) << "skipping cert (verify) " << PORT_GetError();
      continue;
    }

    // Mozilla uses CERT_ImportCerts, which doesn't take a slot arg.  We use
    // PK11_ImportCert instead.
    SECStatus srv = PK11_ImportCert(
        slot,
        cert->os_cert_handle(),
        CK_INVALID_HANDLE,
        net::x509_util::GetUniqueNicknameForSlot(
            cert->GetDefaultNickname(net::CA_CERT),
            &cert->os_cert_handle()->derSubject,
            slot).c_str(),
        PR_FALSE /* includeTrust (unused) */);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      // TODO(mattm): Should we bail or continue on error here?  Mozilla doesn't
      // check error code at all.
      not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_CA_CERT_FAILED));
    }
  }

  // Any errors importing individual certs will be in listed in |not_imported|.
  return true;
}

// Based on nsNSSCertificateDB::ImportServerCertificate.
bool ImportServerCert(
    PK11SlotInfo* slot,
    const net::CertificateList& certificates,
    net::NSSCertDatabase::TrustBits trustBits,
    net::NSSCertDatabase::ImportCertFailureList* not_imported) {
  if (!slot || certificates.empty())
    return false;

  for (size_t i = 0; i < certificates.size(); ++i) {
    const scoped_refptr<net::X509Certificate>& cert = certificates[i];

    // Mozilla uses CERT_ImportCerts, which doesn't take a slot arg.  We use
    // PK11_ImportCert instead.
    SECStatus srv = PK11_ImportCert(
        slot,
        cert->os_cert_handle(),
        CK_INVALID_HANDLE,
        net::x509_util::GetUniqueNicknameForSlot(
            cert->GetDefaultNickname(net::SERVER_CERT),
            &cert->os_cert_handle()->derSubject,
            slot).c_str(),
        PR_FALSE /* includeTrust (unused) */);
    if (srv != SECSuccess) {
      LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
      not_imported->push_back(net::NSSCertDatabase::ImportCertFailure(
          cert, net::ERR_IMPORT_SERVER_CERT_FAILED));
      continue;
    }
  }

  SetCertTrust(certificates[0].get(), net::SERVER_CERT, trustBits);
  // TODO(mattm): Report SetCertTrust result?  Putting in not_imported
  // wouldn't quite match up since it was imported...

  // Any errors importing individual certs will be in listed in |not_imported|.
  return true;
}

// Based on nsNSSCertificateDB::ImportUserCertificate.
int ImportUserCert(const net::CertificateList& certificates) {
  if (certificates.empty())
    return net::ERR_CERT_INVALID;

  const scoped_refptr<net::X509Certificate>& cert = certificates[0];
  CK_OBJECT_HANDLE key;
  crypto::ScopedPK11Slot slot(
      PK11_KeyForCertExists(cert->os_cert_handle(), &key, NULL));

  if (!slot.get())
    return net::ERR_NO_PRIVATE_KEY_FOR_CERT;

  // Mozilla uses CERT_ImportCerts, which doesn't take a slot arg.  We use
  // PK11_ImportCert instead.
  SECStatus srv =
      PK11_ImportCert(slot.get(), cert->os_cert_handle(), key,
                      net::x509_util::GetUniqueNicknameForSlot(
                          cert->GetDefaultNickname(net::USER_CERT),
                          &cert->os_cert_handle()->derSubject, slot.get())
                          .c_str(),
                      PR_FALSE /* includeTrust (unused) */);

  if (srv != SECSuccess) {
    LOG(ERROR) << "PK11_ImportCert failed with error " << PORT_GetError();
    return net::ERR_ADD_USER_CERT_FAILED;
  }

  return net::OK;
}

// Based on nsNSSCertificateDB::SetCertTrust.
bool
SetCertTrust(const net::X509Certificate* cert,
             net::CertType type,
             net::NSSCertDatabase::TrustBits trustBits)
{
  const unsigned kSSLTrustBits = net::NSSCertDatabase::TRUSTED_SSL |
      net::NSSCertDatabase::DISTRUSTED_SSL;
  const unsigned kEmailTrustBits = net::NSSCertDatabase::TRUSTED_EMAIL |
      net::NSSCertDatabase::DISTRUSTED_EMAIL;
  const unsigned kObjSignTrustBits = net::NSSCertDatabase::TRUSTED_OBJ_SIGN |
      net::NSSCertDatabase::DISTRUSTED_OBJ_SIGN;
  if ((trustBits & kSSLTrustBits) == kSSLTrustBits ||
      (trustBits & kEmailTrustBits) == kEmailTrustBits ||
      (trustBits & kObjSignTrustBits) == kObjSignTrustBits) {
    LOG(ERROR) << "SetCertTrust called with conflicting trust bits "
               << trustBits;
    NOTREACHED();
    return false;
  }

  SECStatus srv;
  CERTCertificate *nsscert = cert->os_cert_handle();
  if (type == net::CA_CERT) {
    // Note that we start with CERTDB_VALID_CA for default trust and explicit
    // trust, but explicitly distrusted usages will be set to
    // CERTDB_TERMINAL_RECORD only.
    CERTCertTrust trust = {CERTDB_VALID_CA, CERTDB_VALID_CA, CERTDB_VALID_CA};

    if (trustBits & net::NSSCertDatabase::DISTRUSTED_SSL)
      trust.sslFlags = CERTDB_TERMINAL_RECORD;
    else if (trustBits & net::NSSCertDatabase::TRUSTED_SSL)
      trust.sslFlags |= CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;

    if (trustBits & net::NSSCertDatabase::DISTRUSTED_EMAIL)
      trust.emailFlags = CERTDB_TERMINAL_RECORD;
    else if (trustBits & net::NSSCertDatabase::TRUSTED_EMAIL)
      trust.emailFlags |= CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;

    if (trustBits & net::NSSCertDatabase::DISTRUSTED_OBJ_SIGN)
      trust.objectSigningFlags = CERTDB_TERMINAL_RECORD;
    else if (trustBits & net::NSSCertDatabase::TRUSTED_OBJ_SIGN)
      trust.objectSigningFlags |= CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA;

    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), nsscert, &trust);
  } else if (type == net::SERVER_CERT) {
    CERTCertTrust trust = {0};
    // We only modify the sslFlags, so copy the other flags.
    CERT_GetCertTrust(nsscert, &trust);
    trust.sslFlags = 0;

    if (trustBits & net::NSSCertDatabase::DISTRUSTED_SSL)
      trust.sslFlags |= CERTDB_TERMINAL_RECORD;
    else if (trustBits & net::NSSCertDatabase::TRUSTED_SSL)
      trust.sslFlags |= CERTDB_TRUSTED | CERTDB_TERMINAL_RECORD;

    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), nsscert, &trust);
  } else {
    // ignore user and email/unknown certs
    return true;
  }
  if (srv != SECSuccess)
    LOG(ERROR) << "SetCertTrust failed with error " << PORT_GetError();
  return srv == SECSuccess;
}

}  // namespace mozilla_security_manager
