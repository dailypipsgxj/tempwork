// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_certificate/cast_crl.h"

#include <unordered_map>
#include <unordered_set>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/cast_certificate/proto/revocation.pb.h"
#include "crypto/sha2.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/path_builder.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/signature_policy.h"
#include "net/cert/internal/trust_store.h"
#include "net/cert/internal/verify_certificate_chain.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/cert/x509_certificate.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/parse_values.h"

namespace cast_certificate {
namespace {

enum CrlVersion {
  // version 0: Spki Hash Algorithm = SHA-256
  //            Signature Algorithm = RSA-PKCS1 V1.5 with SHA-256
  CRL_VERSION_0 = 0,
};

// -------------------------------------------------------------------------
// Cast CRL trust anchors.
// -------------------------------------------------------------------------

// There is one trusted root for Cast CRL certificate chains:
//
//   (1) CN=Cast CRL Root CA    (kCastCRLRootCaDer)
//
// These constants are defined by the file included next:

#include "components/cast_certificate/cast_crl_root_ca_cert_der-inc.h"

// Singleton for the Cast CRL trust store.
class CastCRLTrustStore {
 public:
  static CastCRLTrustStore* GetInstance() {
    return base::Singleton<CastCRLTrustStore, base::LeakySingletonTraits<
                                                  CastCRLTrustStore>>::get();
  }

  static net::TrustStore& Get() { return GetInstance()->store_; }

 private:
  friend struct base::DefaultSingletonTraits<CastCRLTrustStore>;

  CastCRLTrustStore() {
    // Initialize the trust store with the root certificate.
    // TODO(ryanchung): Add official Cast CRL Root here
    // scoped_refptr<net::ParsedCertificate> root = net::ParsedCertificate::
    //     net::ParsedCertificate::CreateFromCertificateData(
    //         kCastCRLRootCaDer, sizeof(kCastCRLRootCaDer),
    //         net::ParsedCertificate::DataSource::EXTERNAL_REFERENCE, {});
    // CHECK(root);
    // store_.AddTrustedCertificate(std::move(root));
  }

  net::TrustStore store_;
  DISALLOW_COPY_AND_ASSIGN(CastCRLTrustStore);
};

// Converts a uint64_t unix timestamp to net::der::GeneralizedTime.
bool ConvertTimeSeconds(uint64_t seconds,
                        net::der::GeneralizedTime* generalized_time) {
  base::Time unix_timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(base::saturated_cast<int64_t>(seconds));
  return net::der::EncodeTimeAsGeneralizedTime(unix_timestamp,
                                               generalized_time);
}

// Specifies the signature verification policy.
// The required algorithms are:
// RSASSA PKCS#1 v1.5 with SHA-256, using RSA keys 2048-bits or longer.
std::unique_ptr<net::SignaturePolicy> CreateCastSignaturePolicy() {
  return base::WrapUnique(new net::SimpleSignaturePolicy(2048));
}

// Verifies the CRL is signed by a trusted CRL authority at the time the CRL
// was issued. Verifies the signature of |tbs_crl| is valid based on the
// certificate and signature in |crl|. The validity of |tbs_crl| is verified
// at |time|. The validity period of the CRL is adjusted to be the earliest
// of the issuer certificate chain's expiration and the CRL's expiration and
// the result is stored in |overall_not_after|.
bool VerifyCRL(const Crl& crl,
               const TbsCrl& tbs_crl,
               const base::Time& time,
               net::der::GeneralizedTime* overall_not_after) {
  // Verify the trust of the CRL authority.
  scoped_refptr<net::ParsedCertificate> parsed_cert =
      net::ParsedCertificate::CreateFromCertificateData(
          reinterpret_cast<const uint8_t*>(crl.signer_cert().data()),
          crl.signer_cert().size(),
          net::ParsedCertificate::DataSource::EXTERNAL_REFERENCE, {});
  if (parsed_cert == nullptr) {
    VLOG(2) << "CRL - Issuer certificate parsing failed.";
    return false;
  }

  // Wrap the signature in a BitString.
  net::der::BitString signature_value_bit_string = net::der::BitString(
      net::der::Input(base::StringPiece(crl.signature())), 0);

  // Verify the signature.
  auto signature_policy = CreateCastSignaturePolicy();
  std::unique_ptr<net::SignatureAlgorithm> signature_algorithm_type =
      net::SignatureAlgorithm::CreateRsaPkcs1(net::DigestAlgorithm::Sha256);
  if (!VerifySignedData(*signature_algorithm_type,
                        net::der::Input(&crl.tbs_crl()),
                        signature_value_bit_string, parsed_cert->tbs().spki_tlv,
                        signature_policy.get())) {
    VLOG(2) << "CRL - Signature verification failed.";
    return false;
  }

  // Verify the issuer certificate.
  net::der::GeneralizedTime verification_time;
  if (!net::der::EncodeTimeAsGeneralizedTime(time, &verification_time)) {
    VLOG(2) << "CRL - Unable to parse verification time.";
    return false;
  }
  net::CertPathBuilder::Result result;
  net::CertPathBuilder path_builder(
      parsed_cert.get(), &CastCRLTrustStore::Get(), signature_policy.get(),
      verification_time, &result);
  net::CompletionStatus rv = path_builder.Run(base::Closure());
  DCHECK_EQ(rv, net::CompletionStatus::SYNC);
  if (!result.is_success() || result.paths.empty() ||
      !result.paths[result.best_result_index]->is_success()) {
    VLOG(2) << "CRL - Issuer certificate verification failed.";
    return false;
  }
  // There are no requirements placed on the leaf certificate having any
  // particular KeyUsages. Leaf certificate checks are bypassed.

  // Verify the CRL is still valid.
  net::der::GeneralizedTime not_before;
  if (!ConvertTimeSeconds(tbs_crl.not_before_seconds(), &not_before)) {
    VLOG(2) << "CRL - Unable to parse not_before.";
    return false;
  }
  net::der::GeneralizedTime not_after;
  if (!ConvertTimeSeconds(tbs_crl.not_after_seconds(), &not_after)) {
    VLOG(2) << "CRL - Unable to parse not_after.";
    return false;
  }
  if ((verification_time < not_before) || (verification_time > not_after)) {
    VLOG(2) << "CRL - Not time-valid.";
    return false;
  }

  // Set CRL expiry to the earliest of the cert chain expiry and CRL expiry.
  *overall_not_after = not_after;
  for (const auto& cert : result.paths[result.best_result_index]->path) {
    net::der::GeneralizedTime cert_not_after = cert->tbs().validity_not_after;
    if (cert_not_after < *overall_not_after)
      *overall_not_after = cert_not_after;
  }

  // Perform sanity check on serial numbers.
  for (const auto& range : tbs_crl.revoked_serial_number_ranges()) {
    uint64_t first_serial_number = range.first_serial_number();
    uint64_t last_serial_number = range.last_serial_number();
    if (last_serial_number < first_serial_number) {
      VLOG(2) << "CRL - Malformed serial number range.";
      return false;
    }
  }
  return true;
}

class CastCRLImpl : public CastCRL {
 public:
  CastCRLImpl(const TbsCrl& tbs_crl,
              const net::der::GeneralizedTime& overall_not_after);
  ~CastCRLImpl() override;

  bool CheckRevocation(const net::ParsedCertificateList& trusted_chain,
                       const base::Time& time) const override;

 private:
  struct SerialNumberRange {
    uint64_t first_serial;
    uint64_t last_serial;
  };

  net::der::GeneralizedTime not_before_;
  net::der::GeneralizedTime not_after_;

  // Revoked public key hashes.
  // The values consist of the SHA256 hash of the SubjectPublicKeyInfo.
  std::set<std::string> revoked_hashes_;

  // Revoked serial number ranges indexed by issuer public key hash.
  // The key is the SHA256 hash of issuer's SubjectPublicKeyInfo.
  // The value is a list of revoked serial number ranges.
  std::unordered_map<std::string, std::vector<SerialNumberRange>>
      revoked_serial_numbers_;
  DISALLOW_COPY_AND_ASSIGN(CastCRLImpl);
};

CastCRLImpl::CastCRLImpl(const TbsCrl& tbs_crl,
                         const net::der::GeneralizedTime& overall_not_after) {
  // Parse the validity information.
  // Assume ConvertTimeSeconds will succeed. Successful call to VerifyCRL
  // means that these calls were successful.
  ConvertTimeSeconds(tbs_crl.not_before_seconds(), &not_before_);
  ConvertTimeSeconds(tbs_crl.not_after_seconds(), &not_after_);
  if (overall_not_after < not_after_)
    not_after_ = overall_not_after;

  // Parse the revoked hashes.
  for (const auto& hash : tbs_crl.revoked_public_key_hashes()) {
    revoked_hashes_.insert(hash);
  }

  // Parse the revoked serial ranges.
  for (const auto& range : tbs_crl.revoked_serial_number_ranges()) {
    std::string issuer_hash = range.issuer_public_key_hash();

    uint64_t first_serial_number = range.first_serial_number();
    uint64_t last_serial_number = range.last_serial_number();
    auto& serial_number_range = revoked_serial_numbers_[issuer_hash];
    serial_number_range.push_back({first_serial_number, last_serial_number});
  }
}

CastCRLImpl::~CastCRLImpl() {}

// Verifies the revocation status of the certificate chain, at the specified
// time.
bool CastCRLImpl::CheckRevocation(
    const net::ParsedCertificateList& trusted_chain,
    const base::Time& time) const {
  if (trusted_chain.empty())
    return false;

  // Check the validity of the CRL at the specified time.
  net::der::GeneralizedTime verification_time;
  if (!net::der::EncodeTimeAsGeneralizedTime(time, &verification_time)) {
    VLOG(2) << "CRL verification time malformed.";
    return false;
  }
  if ((verification_time < not_before_) || (verification_time > not_after_)) {
    VLOG(2) << "CRL not time-valid. Perform hard fail.";
    return false;
  }

  // Check revocation.
  for (size_t i = 0; i < trusted_chain.size(); ++i) {
    const auto& parsed_cert = trusted_chain[i];
    // Calculate the public key's hash to check for revocation.
    std::string spki_hash =
        crypto::SHA256HashString(parsed_cert->tbs().spki_tlv.AsString());
    if (revoked_hashes_.find(spki_hash) != revoked_hashes_.end()) {
      VLOG(2) << "Public key is revoked.";
      return false;
    }

    // Check if the subordinate certificate was revoked by serial number.
    if (i > 0) {
      auto issuer_iter = revoked_serial_numbers_.find(spki_hash);
      if (issuer_iter != revoked_serial_numbers_.end()) {
        const auto& subordinate = trusted_chain[i - 1];
        uint64_t serial_number;
        // Only Google generated device certificates will be revoked by range.
        // These will always be less than 64 bits in length.
        if (!net::der::ParseUint64(subordinate->tbs().serial_number,
                                   &serial_number)) {
          continue;
        }
        for (const auto& revoked_serial : issuer_iter->second) {
          if (revoked_serial.first_serial <= serial_number &&
              revoked_serial.last_serial >= serial_number) {
            VLOG(2) << "Serial number is revoked";
            return false;
          }
        }
      }
    }
  }
  return true;
}

}  // namespace

std::unique_ptr<CastCRL> ParseAndVerifyCRL(const std::string& crl_proto,
                                           const base::Time& time) {
  CrlBundle crl_bundle;
  if (!crl_bundle.ParseFromString(crl_proto)) {
    LOG(ERROR) << "CRL - Binary could not be parsed.";
    return nullptr;
  }
  for (auto const& crl : crl_bundle.crls()) {
    TbsCrl tbs_crl;
    if (!tbs_crl.ParseFromString(crl.tbs_crl())) {
      LOG(WARNING) << "Binary TBS CRL could not be parsed.";
      continue;
    }
    if (tbs_crl.version() != CRL_VERSION_0) {
      continue;
    }
    net::der::GeneralizedTime overall_not_after;
    if (!VerifyCRL(crl, tbs_crl, time, &overall_not_after)) {
      LOG(ERROR) << "CRL - Verification failed.";
      return nullptr;
    }
    return base::WrapUnique(new CastCRLImpl(tbs_crl, overall_not_after));
  }
  LOG(ERROR) << "No supported version of revocation data.";
  return nullptr;
}

bool SetCRLTrustAnchorForTest(const std::string& cert) {
  scoped_refptr<net::ParsedCertificate> anchor(
      net::ParsedCertificate::CreateFromCertificateCopy(cert, {}));
  if (!anchor)
    return false;
  CastCRLTrustStore::Get().Clear();
  CastCRLTrustStore::Get().AddTrustedCertificate(std::move(anchor));
  return true;
}

}  // namespace cast_certificate
