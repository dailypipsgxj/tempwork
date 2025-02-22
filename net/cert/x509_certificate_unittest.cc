// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS_CERTS)
#include <cert.h>
#endif

using base::HexEncode;
using base::Time;

namespace net {

// Certificates for test data. They're obtained with:
//
// $ openssl s_client -connect [host]:443 -showcerts > /tmp/host.pem < /dev/null
// $ openssl x509 -inform PEM -outform DER < /tmp/host.pem > /tmp/host.der
//
// For fingerprint
// $ openssl x509 -inform DER -fingerprint -noout < /tmp/host.der

// For valid_start, valid_expiry
// $ openssl x509 -inform DER -text -noout < /tmp/host.der |
//    grep -A 2 Validity
// $ date +%s -d '<date str>'

// Google's cert.
SHA256HashValue google_fingerprint = {
    {0x21, 0xaf, 0x58, 0x74, 0xea, 0x6b, 0xad, 0xbd, 0xe4, 0xb3, 0xb1,
     0xaa, 0x53, 0x32, 0x80, 0x8f, 0xbf, 0x8a, 0x24, 0x7d, 0x98, 0xec,
     0x7f, 0x77, 0x49, 0x38, 0x42, 0x81, 0x26, 0x7f, 0xed, 0x38}};

// The fingerprint of the Google certificate used in the parsing tests,
// which is newer than the one included in the x509_certificate_data.h
SHA256HashValue google_parse_fingerprint = {
    {0xf6, 0x41, 0xc3, 0x6c, 0xfe, 0xf4, 0x9b, 0xc0, 0x71, 0x35, 0x9e,
     0xcf, 0x88, 0xee, 0xd9, 0x31, 0x7b, 0x73, 0x8b, 0x59, 0x89, 0x41,
     0x6a, 0xd4, 0x01, 0x72, 0x0c, 0x0a, 0x4e, 0x2e, 0x63, 0x52}};

// The fingerprint for the Thawte SGC certificate
SHA256HashValue thawte_parse_fingerprint = {
    {0x10, 0x85, 0xa6, 0xf4, 0x54, 0xd0, 0xc9, 0x11, 0x98, 0xfd, 0xda,
     0xb1, 0x1a, 0x31, 0xc7, 0x16, 0xd5, 0xdc, 0xd6, 0x8d, 0xf9, 0x1c,
     0x03, 0x9c, 0xe1, 0x8d, 0xca, 0x9b, 0xeb, 0x3c, 0xde, 0x3d}};

// Dec 18 00:00:00 2009 GMT
const double kGoogleParseValidFrom = 1261094400;
// Dec 18 23:59:59 2011 GMT
const double kGoogleParseValidTo = 1324252799;

void CheckGoogleCert(const scoped_refptr<X509Certificate>& google_cert,
                     const SHA256HashValue& expected_fingerprint,
                     double valid_from,
                     double valid_to) {
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert.get());

  const CertPrincipal& subject = google_cert->subject();
  EXPECT_EQ("www.google.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Google Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = google_cert->issuer();
  EXPECT_EQ("Thawte SGC CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("ZA", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("Thawte Consulting (Pty) Ltd.", issuer.organization_names[0]);
  EXPECT_EQ(0U, issuer.organization_unit_names.size());
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = google_cert->valid_start();
  EXPECT_EQ(valid_from, valid_start.ToDoubleT());

  const Time& valid_expiry = google_cert->valid_expiry();
  EXPECT_EQ(valid_to, valid_expiry.ToDoubleT());

  EXPECT_EQ(expected_fingerprint, X509Certificate::CalculateFingerprint256(
                                      google_cert->os_cert_handle()));

  std::vector<std::string> dns_names;
  google_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.google.com", dns_names[0]);
}

TEST(X509CertificateTest, GoogleCertParsing) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  CheckGoogleCert(google_cert, google_fingerprint,
                  1238192407,   // Mar 27 22:20:07 2009 GMT
                  1269728407);  // Mar 27 22:20:07 2010 GMT
}

TEST(X509CertificateTest, WebkitCertParsing) {
  scoped_refptr<X509Certificate> webkit_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), webkit_cert.get());

  const CertPrincipal& subject = webkit_cert->subject();
  EXPECT_EQ("Cupertino", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Apple Inc.", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Mac OS Forge", subject.organization_unit_names[0]);
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = webkit_cert->issuer();
  EXPECT_EQ("Go Daddy Secure Certification Authority", issuer.common_name);
  EXPECT_EQ("Scottsdale", issuer.locality_name);
  EXPECT_EQ("Arizona", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("GoDaddy.com, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("http://certificates.godaddy.com/repository",
      issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = webkit_cert->valid_start();
  EXPECT_EQ(1205883319, valid_start.ToDoubleT());  // Mar 18 23:35:19 2008 GMT

  const Time& valid_expiry = webkit_cert->valid_expiry();
  EXPECT_EQ(1300491319, valid_expiry.ToDoubleT());  // Mar 18 23:35:19 2011 GMT

  std::vector<std::string> dns_names;
  webkit_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(2U, dns_names.size());
  EXPECT_EQ("*.webkit.org", dns_names[0]);
  EXPECT_EQ("webkit.org", dns_names[1]);

  // Test that the wildcard cert matches properly.
  bool unused = false;
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("www.webkit.org", &unused));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("foo.webkit.org", &unused));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("webkit.org", &unused));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.webkit.com", &unused));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.foo.webkit.com", &unused));
}

TEST(X509CertificateTest, ThawteCertParsing) {
  scoped_refptr<X509Certificate> thawte_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), thawte_cert.get());

  const CertPrincipal& subject = thawte_cert->subject();
  EXPECT_EQ("www.thawte.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Thawte Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = thawte_cert->issuer();
  EXPECT_EQ("thawte Extended Validation SSL CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("thawte, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("Terms of use at https://www.thawte.com/cps (c)06",
            issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = thawte_cert->valid_start();
  EXPECT_EQ(1227052800, valid_start.ToDoubleT());  // Nov 19 00:00:00 2008 GMT

  const Time& valid_expiry = thawte_cert->valid_expiry();
  EXPECT_EQ(1263772799, valid_expiry.ToDoubleT());  // Jan 17 23:59:59 2010 GMT

  std::vector<std::string> dns_names;
  thawte_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.thawte.com", dns_names[0]);
}

// Test that all desired AttributeAndValue pairs can be extracted when only
// a single RelativeDistinguishedName is present. "Normally" there is only
// one AVA per RDN, but some CAs place all AVAs within a single RDN.
// This is a regression test for http://crbug.com/101009
TEST(X509CertificateTest, MultivalueRDN) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> multivalue_rdn_cert =
      ImportCertFromFile(certs_dir, "multivalue_rdn.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), multivalue_rdn_cert.get());

  const CertPrincipal& subject = multivalue_rdn_cert->subject();
  EXPECT_EQ("Multivalue RDN Test", subject.common_name);
  EXPECT_EQ("", subject.locality_name);
  EXPECT_EQ("", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Chromium net_unittests", subject.organization_unit_names[0]);
  ASSERT_EQ(1U, subject.domain_components.size());
  EXPECT_EQ("Chromium", subject.domain_components[0]);
}

// Test that characters which would normally be escaped in the string form,
// such as '=' or '"', are not escaped when parsed as individual components.
// This is a regression test for http://crbug.com/102839
TEST(X509CertificateTest, UnescapedSpecialCharacters) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> unescaped_cert =
      ImportCertFromFile(certs_dir, "unescaped.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unescaped_cert.get());

  const CertPrincipal& subject = unescaped_cert->subject();
  EXPECT_EQ("127.0.0.1", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.street_addresses.size());
  EXPECT_EQ("1600 Amphitheatre Parkway", subject.street_addresses[0]);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium = \"net_unittests\"", subject.organization_names[0]);
  ASSERT_EQ(2U, subject.organization_unit_names.size());
  EXPECT_EQ("net_unittests", subject.organization_unit_names[0]);
  EXPECT_EQ("Chromium", subject.organization_unit_names[1]);
  EXPECT_EQ(0U, subject.domain_components.size());
}

TEST(X509CertificateTest, SerialNumbers) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  static const uint8_t google_serial[16] = {
    0x01,0x2a,0x39,0x76,0x0d,0x3f,0x4f,0xc9,
    0x0b,0xe7,0xbd,0x2b,0xcf,0x95,0x2e,0x7a,
  };

  ASSERT_EQ(sizeof(google_serial), google_cert->serial_number().size());
  EXPECT_TRUE(memcmp(google_cert->serial_number().data(), google_serial,
                     sizeof(google_serial)) == 0);

  // We also want to check a serial number where the first byte is >= 0x80 in
  // case the underlying library tries to pad it.
  scoped_refptr<X509Certificate> paypal_null_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  static const uint8_t paypal_null_serial[3] = {0x00, 0xf0, 0x9b};
  ASSERT_EQ(sizeof(paypal_null_serial),
            paypal_null_cert->serial_number().size());
  EXPECT_TRUE(memcmp(paypal_null_cert->serial_number().data(),
                     paypal_null_serial, sizeof(paypal_null_serial)) == 0);
}

TEST(X509CertificateTest, SHA256FingerprintsCorrectly) {
  scoped_refptr<X509Certificate> google_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  const SHA256HashValue google_sha256_fingerprint = {
      {0x21, 0xaf, 0x58, 0x74, 0xea, 0x6b, 0xad, 0xbd, 0xe4, 0xb3, 0xb1,
       0xaa, 0x53, 0x32, 0x80, 0x8f, 0xbf, 0x8a, 0x24, 0x7d, 0x98, 0xec,
       0x7f, 0x77, 0x49, 0x38, 0x42, 0x81, 0x26, 0x7f, 0xed, 0x38}};

  EXPECT_EQ(google_sha256_fingerprint, X509Certificate::CalculateFingerprint256(
                                           google_cert->os_cert_handle()));
}

TEST(X509CertificateTest, CAFingerprints) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert.get());

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert1.get());

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert2.get());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert1->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  intermediates.clear();
  intermediates.push_back(intermediate_cert2->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  // No intermediate CA certicates.
  intermediates.clear();
  scoped_refptr<X509Certificate> cert_chain3 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  SHA256HashValue cert_chain1_ca_fingerprint_256 = {
      {0x51, 0x15, 0x30, 0x49, 0x97, 0x54, 0xf8, 0xb4, 0x17, 0x41, 0x6b,
       0x58, 0x78, 0xb0, 0x89, 0xd2, 0xc3, 0xae, 0x66, 0xc1, 0x16, 0x80,
       0xa0, 0x78, 0xe7, 0x53, 0x45, 0xa2, 0xfb, 0x80, 0xe1, 0x07}};
  SHA256HashValue cert_chain2_ca_fingerprint_256 = {
      {0x00, 0xbd, 0x2b, 0x0e, 0xdd, 0x83, 0x40, 0xb1, 0x74, 0x6c, 0xc3,
       0x95, 0xc0, 0xe3, 0x55, 0xb2, 0x16, 0x58, 0x53, 0xfd, 0xb9, 0x3c,
       0x52, 0xda, 0xdd, 0xa8, 0x22, 0x8b, 0x07, 0x00, 0x2d, 0xce}};
  // The SHA-256 hash of nothing.
  SHA256HashValue cert_chain3_ca_fingerprint_256 = {
      {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
       0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
       0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55}};
  EXPECT_EQ(cert_chain1_ca_fingerprint_256,
            X509Certificate::CalculateCAFingerprint256(
                cert_chain1->GetIntermediateCertificates()));
  EXPECT_EQ(cert_chain2_ca_fingerprint_256,
            X509Certificate::CalculateCAFingerprint256(
                cert_chain2->GetIntermediateCertificates()));
  EXPECT_EQ(cert_chain3_ca_fingerprint_256,
            X509Certificate::CalculateCAFingerprint256(
                cert_chain3->GetIntermediateCertificates()));

  SHA256HashValue cert_chain1_chain_fingerprint_256 = {
      {0xac, 0xff, 0xcc, 0x63, 0x0d, 0xd0, 0xa7, 0x19, 0x78, 0xb5, 0x8a,
       0x47, 0x8b, 0x67, 0x97, 0xcb, 0x8d, 0xe1, 0x6a, 0x8a, 0x57, 0x70,
       0xda, 0x9a, 0x53, 0x72, 0xe2, 0xa0, 0x08, 0xab, 0xcc, 0x8f}};
  SHA256HashValue cert_chain2_chain_fingerprint_256 = {
      {0x67, 0x3a, 0x11, 0x20, 0xd6, 0x94, 0x14, 0xe4, 0x16, 0x9f, 0x58,
       0xe2, 0x8b, 0xf7, 0x27, 0xed, 0xbb, 0xe8, 0xa7, 0xff, 0x1c, 0x8c,
       0x0f, 0x21, 0x38, 0x16, 0x7c, 0xad, 0x1f, 0x22, 0x6f, 0x9b}};
  SHA256HashValue cert_chain3_chain_fingerprint_256 = {
      {0x16, 0x7a, 0xbd, 0xb4, 0x57, 0x04, 0x65, 0x3c, 0x3b, 0xef, 0x6e,
       0x6a, 0xa6, 0x02, 0x73, 0x30, 0x3e, 0x34, 0x1b, 0x43, 0xc2, 0x7c,
       0x98, 0x52, 0x9f, 0x34, 0x7f, 0x55, 0x97, 0xe9, 0x1a, 0x10}};
  EXPECT_EQ(cert_chain1_chain_fingerprint_256,
            X509Certificate::CalculateChainFingerprint256(
                cert_chain1->os_cert_handle(),
                cert_chain1->GetIntermediateCertificates()));
  EXPECT_EQ(cert_chain2_chain_fingerprint_256,
            X509Certificate::CalculateChainFingerprint256(
                cert_chain2->os_cert_handle(),
                cert_chain2->GetIntermediateCertificates()));
  EXPECT_EQ(cert_chain3_chain_fingerprint_256,
            X509Certificate::CalculateChainFingerprint256(
                cert_chain3->os_cert_handle(),
                cert_chain3->GetIntermediateCertificates()));
}

TEST(X509CertificateTest, ParseSubjectAltNames) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> san_cert =
      ImportCertFromFile(certs_dir, "subjectAltName_sanity_check.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), san_cert.get());

  std::vector<std::string> dns_names;
  std::vector<std::string> ip_addresses;
  san_cert->GetSubjectAltName(&dns_names, &ip_addresses);

  // Ensure that DNS names are correctly parsed.
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("test.example", dns_names[0]);

  // Ensure that both IPv4 and IPv6 addresses are correctly parsed.
  ASSERT_EQ(2U, ip_addresses.size());

  static const uint8_t kIPv4Address[] = {
      0x7F, 0x00, 0x00, 0x02
  };
  ASSERT_EQ(arraysize(kIPv4Address), ip_addresses[0].size());
  EXPECT_EQ(0, memcmp(ip_addresses[0].data(), kIPv4Address,
                      arraysize(kIPv4Address)));

  static const uint8_t kIPv6Address[] = {
      0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  ASSERT_EQ(arraysize(kIPv6Address), ip_addresses[1].size());
  EXPECT_EQ(0, memcmp(ip_addresses[1].data(), kIPv6Address,
                      arraysize(kIPv6Address)));

  // Ensure the subjectAltName dirName has not influenced the handling of
  // the subject commonName.
  EXPECT_EQ("127.0.0.1", san_cert->subject().common_name);
}

#if defined(USE_NSS_CERTS)
TEST(X509CertificateTest, ParseClientSubjectAltNames) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // This cert contains one rfc822Name field, and one Microsoft UPN
  // otherName field.
  scoped_refptr<X509Certificate> san_cert =
      ImportCertFromFile(certs_dir, "client_3.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), san_cert.get());

  std::vector<std::string> rfc822_names;
  net::x509_util::GetRFC822SubjectAltNames(san_cert->os_cert_handle(),
                                           &rfc822_names);
  ASSERT_EQ(1U, rfc822_names.size());
  EXPECT_EQ("santest@example.com", rfc822_names[0]);

  std::vector<std::string> upn_names;
  net::x509_util::GetUPNSubjectAltNames(san_cert->os_cert_handle(), &upn_names);
  ASSERT_EQ(1U, upn_names.size());
  EXPECT_EQ("santest@ad.corp.example.com", upn_names[0]);
}
#endif  // defined(USE_NSS_CERTS)

TEST(X509CertificateTest, ExtractSPKIFromDERCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "nist.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert.get());

  std::string derBytes;
  EXPECT_TRUE(X509Certificate::GetDEREncoded(cert->os_cert_handle(),
                                             &derBytes));

  base::StringPiece spkiBytes;
  EXPECT_TRUE(asn1::ExtractSPKIFromDERCert(derBytes, &spkiBytes));

  uint8_t hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(spkiBytes.data()),
                      spkiBytes.size(), hash);

  EXPECT_EQ(0, memcmp(hash, kNistSPKIHash, sizeof(hash)));
}

TEST(X509CertificateTest, ExtractCRLURLsFromDERCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "nist.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert.get());

  std::string derBytes;
  EXPECT_TRUE(X509Certificate::GetDEREncoded(cert->os_cert_handle(),
                                             &derBytes));

  std::vector<base::StringPiece> crl_urls;
  EXPECT_TRUE(asn1::ExtractCRLURLsFromDERCert(derBytes, &crl_urls));

  EXPECT_EQ(1u, crl_urls.size());
  if (crl_urls.size() > 0) {
    EXPECT_EQ("http://SVRSecure-G3-crl.verisign.com/SVRSecureG3.crl",
              crl_urls[0].as_string());
  }
}

// Tests X509CertificateCache via X509Certificate::CreateFromHandle.  We
// call X509Certificate::CreateFromHandle several times and observe whether
// it returns a cached or new OSCertHandle.
TEST(X509CertificateTest, Cache) {
  X509Certificate::OSCertHandle google_cert_handle;
  X509Certificate::OSCertHandle thawte_cert_handle;

  // Add a single certificate to the certificate cache.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert1(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  // Add the same certificate, but as a new handle.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert2(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  // A new X509Certificate should be returned.
  EXPECT_NE(cert1.get(), cert2.get());
  // But both instances should share the underlying OS certificate handle.
  EXPECT_EQ(cert1->os_cert_handle(), cert2->os_cert_handle());
  EXPECT_EQ(0u, cert1->GetIntermediateCertificates().size());
  EXPECT_EQ(0u, cert2->GetIntermediateCertificates().size());

  // Add the same certificate, but this time with an intermediate. This
  // should result in the intermediate being cached. Note that this is not
  // a legitimate chain, but is suitable for testing.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  thawte_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(thawte_cert_handle);
  scoped_refptr<X509Certificate> cert3(X509Certificate::CreateFromHandle(
      google_cert_handle, intermediates));
  X509Certificate::FreeOSCertHandle(google_cert_handle);
  X509Certificate::FreeOSCertHandle(thawte_cert_handle);

  // Test that the new certificate, even with intermediates, results in the
  // same underlying handle being used.
  EXPECT_EQ(cert1->os_cert_handle(), cert3->os_cert_handle());
  // Though they use the same OS handle, the intermediates should be different.
  EXPECT_NE(cert1->GetIntermediateCertificates().size(),
      cert3->GetIntermediateCertificates().size());
}

TEST(X509CertificateTest, Pickle) {
  X509Certificate::OSCertHandle google_cert_handle =
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der));
  X509Certificate::OSCertHandle thawte_cert_handle =
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(thawte_cert_handle);
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
      google_cert_handle, intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert.get());

  X509Certificate::FreeOSCertHandle(google_cert_handle);
  X509Certificate::FreeOSCertHandle(thawte_cert_handle);

  base::Pickle pickle;
  cert->Persist(&pickle);

  base::PickleIterator iter(pickle);
  scoped_refptr<X509Certificate> cert_from_pickle =
      X509Certificate::CreateFromPickle(
          &iter, X509Certificate::PICKLETYPE_CERTIFICATE_CHAIN_V3);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert_from_pickle.get());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      cert->os_cert_handle(), cert_from_pickle->os_cert_handle()));
  const X509Certificate::OSCertHandles& cert_intermediates =
      cert->GetIntermediateCertificates();
  const X509Certificate::OSCertHandles& pickle_intermediates =
      cert_from_pickle->GetIntermediateCertificates();
  ASSERT_EQ(cert_intermediates.size(), pickle_intermediates.size());
  for (size_t i = 0; i < cert_intermediates.size(); ++i) {
    EXPECT_TRUE(X509Certificate::IsSameOSCert(cert_intermediates[i],
                                              pickle_intermediates[i]));
  }
}

TEST(X509CertificateTest, IntermediateCertificates) {
  scoped_refptr<X509Certificate> webkit_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  scoped_refptr<X509Certificate> thawte_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  X509Certificate::OSCertHandle google_handle;
  // Create object with no intermediates:
  google_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  X509Certificate::OSCertHandles intermediates1;
  scoped_refptr<X509Certificate> cert1;
  cert1 = X509Certificate::CreateFromHandle(google_handle, intermediates1);
  EXPECT_EQ(0u, cert1->GetIntermediateCertificates().size());

  // Create object with 2 intermediates:
  X509Certificate::OSCertHandles intermediates2;
  intermediates2.push_back(webkit_cert->os_cert_handle());
  intermediates2.push_back(thawte_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert2;
  cert2 = X509Certificate::CreateFromHandle(google_handle, intermediates2);

  // Verify it has all the intermediates:
  const X509Certificate::OSCertHandles& cert2_intermediates =
      cert2->GetIntermediateCertificates();
  ASSERT_EQ(2u, cert2_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert2_intermediates[0],
                                            webkit_cert->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert2_intermediates[1],
                                            thawte_cert->os_cert_handle()));

  // Cleanup
  X509Certificate::FreeOSCertHandle(google_handle);
}

TEST(X509CertificateTest, IsIssuedByEncoded) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Test a client certificate from MIT.
  scoped_refptr<X509Certificate> mit_davidben_cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), mit_davidben_cert.get());

  std::string mit_issuer(reinterpret_cast<const char*>(MITDN),
                         sizeof(MITDN));

  // Test a certificate from Google, issued by Thawte
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert.get());

  std::string thawte_issuer(reinterpret_cast<const char*>(ThawteDN),
                            sizeof(ThawteDN));

  // Check that the David Ben certificate is issued by MIT, but not
  // by Thawte.
  std::vector<std::string> issuers;
  issuers.clear();
  issuers.push_back(mit_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_FALSE(google_cert->IsIssuedByEncoded(issuers));

  // Check that the Google certificate is issued by Thawte and not
  // by MIT.
  issuers.clear();
  issuers.push_back(thawte_issuer);
  EXPECT_FALSE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));

  // Check that they both pass when given a list of the two issuers.
  issuers.clear();
  issuers.push_back(mit_issuer);
  issuers.push_back(thawte_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));
}

TEST(X509CertificateTest, IsSelfSigned) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(cert->os_cert_handle()));

  scoped_refptr<X509Certificate> self_signed(
      ImportCertFromFile(certs_dir, "aia-root.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), self_signed.get());
  EXPECT_TRUE(X509Certificate::IsSelfSigned(self_signed->os_cert_handle()));

  scoped_refptr<X509Certificate> bad_name(
      ImportCertFromFile(certs_dir, "self-signed-invalid-name.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), bad_name.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(bad_name->os_cert_handle()));

  scoped_refptr<X509Certificate> bad_sig(
      ImportCertFromFile(certs_dir, "self-signed-invalid-sig.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), bad_sig.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(bad_sig->os_cert_handle()));
}

TEST(X509CertificateTest, IsIssuedByEncodedWithIntermediates) {
  static const unsigned char kPolicyRootDN[] = {
    0x30, 0x1e, 0x31, 0x1c, 0x30, 0x1a, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x13, 0x50, 0x6f, 0x6c, 0x69, 0x63, 0x79, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41
  };
  static const unsigned char kPolicyIntermediateDN[] = {
    0x30, 0x26, 0x31, 0x24, 0x30, 0x22, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x1b, 0x50, 0x6f, 0x6c, 0x69, 0x63, 0x79, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x20, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6d, 0x65, 0x64, 0x69, 0x61, 0x74,
    0x65, 0x20, 0x43, 0x41
  };

  base::FilePath certs_dir = GetTestCertsDirectory();

  CertificateList policy_chain = CreateCertificateListFromFile(
      certs_dir, "explicit-policy-chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3u, policy_chain.size());

  // The intermediate CA certificate's policyConstraints extension has a
  // requireExplicitPolicy field with SkipCerts=0.
  std::string policy_intermediate_dn(
      reinterpret_cast<const char*>(kPolicyIntermediateDN),
      sizeof(kPolicyIntermediateDN));
  std::string policy_root_dn(reinterpret_cast<const char*>(kPolicyRootDN),
                             sizeof(kPolicyRootDN));

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(policy_chain[1]->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(policy_chain[0]->os_cert_handle(),
                                        intermediates);

  std::vector<std::string> issuers;

  // Check that the chain is issued by the intermediate.
  issuers.clear();
  issuers.push_back(policy_intermediate_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is also issued by the root.
  issuers.clear();
  issuers.push_back(policy_root_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is issued by either the intermediate or the root.
  issuers.clear();
  issuers.push_back(policy_intermediate_dn);
  issuers.push_back(policy_root_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that an empty issuers list returns false.
  issuers.clear();
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is not issued by Verisign
  std::string mit_issuer(reinterpret_cast<const char*>(VerisignDN),
                         sizeof(VerisignDN));
  issuers.clear();
  issuers.push_back(mit_issuer);
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded(issuers));
}

// Tests that FreeOSCertHandle ignores NULL on each OS.
TEST(X509CertificateTest, FreeNullHandle) {
  X509Certificate::FreeOSCertHandle(NULL);
}

#if defined(USE_NSS_CERTS)
TEST(X509CertificateTest, GetDefaultNickname) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "no_subject_common_name_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert.get());

  std::string nickname = test_cert->GetDefaultNickname(USER_CERT);
  EXPECT_EQ("wtc@google.com's COMODO Client Authentication and "
            "Secure Email CA ID", nickname);
}
#endif

const struct CertificateFormatTestData {
  const char* file_name;
  X509Certificate::Format format;
  SHA256HashValue* chain_fingerprints[3];
} kFormatTestData[] = {
    // DER Parsing - single certificate, DER encoded
    {"google.single.der",
     X509Certificate::FORMAT_SINGLE_CERTIFICATE,
     {
         &google_parse_fingerprint, NULL,
     }},
    // DER parsing - single certificate, PEM encoded
    {"google.single.pem",
     X509Certificate::FORMAT_SINGLE_CERTIFICATE,
     {
         &google_parse_fingerprint, NULL,
     }},
    // PEM parsing - single certificate, PEM encoded with a PEB of
    // "CERTIFICATE"
    {"google.single.pem",
     X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
     {
         &google_parse_fingerprint, NULL,
     }},
    // PEM parsing - sequence of certificates, PEM encoded with a PEB of
    // "CERTIFICATE"
    {"google.chain.pem",
     X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, DER
    // encoding
    {"google.binary.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
    // encoded with a PEM PEB of "CERTIFICATE"
    {"google.pem_cert.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
    // encoded with a PEM PEB of "PKCS7"
    {"google.pem_pkcs7.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    // All of the above, this time using auto-detection
    {"google.single.der",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, NULL,
     }},
    {"google.single.pem",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, NULL,
     }},
    {"google.chain.pem",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    {"google.binary.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    {"google.pem_cert.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
    {"google.pem_pkcs7.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint, &thawte_parse_fingerprint, NULL,
     }},
};

class X509CertificateParseTest
    : public testing::TestWithParam<CertificateFormatTestData> {
 public:
  virtual ~X509CertificateParseTest() {}
  void SetUp() override { test_data_ = GetParam(); }
  void TearDown() override {}

 protected:
  CertificateFormatTestData test_data_;
};

TEST_P(X509CertificateParseTest, CanParseFormat) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, test_data_.file_name, test_data_.format);
  ASSERT_FALSE(certs.empty());
  ASSERT_LE(certs.size(), arraysize(test_data_.chain_fingerprints));
  CheckGoogleCert(certs.front(), google_parse_fingerprint,
                  kGoogleParseValidFrom, kGoogleParseValidTo);

  size_t i;
  for (i = 0; i < arraysize(test_data_.chain_fingerprints); ++i) {
    if (test_data_.chain_fingerprints[i] == NULL) {
      // No more test certificates expected - make sure no more were
      // returned before marking this test a success.
      EXPECT_EQ(i, certs.size());
      break;
    }

    // A cert is expected - make sure that one was parsed.
    ASSERT_LT(i, certs.size());

    // Compare the parsed certificate with the expected certificate, by
    // comparing fingerprints.
    EXPECT_EQ(
        *test_data_.chain_fingerprints[i],
        X509Certificate::CalculateFingerprint256(certs[i]->os_cert_handle()));
  }
}

INSTANTIATE_TEST_CASE_P(, X509CertificateParseTest,
                        testing::ValuesIn(kFormatTestData));

struct CertificateNameVerifyTestData {
  // true iff we expect hostname to match an entry in cert_names.
  bool expected;
  // The hostname to match.
  const char* hostname;
  // Common name, may be used if |dns_names| or |ip_addrs| are empty.
  const char* common_name;
  // Comma separated list of certificate names to match against. Any occurrence
  // of '#' will be replaced with a null character before processing.
  const char* dns_names;
  // Comma separated list of certificate IP Addresses to match against. Each
  // address is x prefixed 16 byte hex code for v6 or dotted-decimals for v4.
  const char* ip_addrs;
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const CertificateNameVerifyTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname && data.common_name);
  // Using StringPiece to allow for optional fields being NULL.
  *os << " expected: " << data.expected
      << "; hostname: " << data.hostname
      << "; common_name: " << data.common_name
      << "; dns_names: " << base::StringPiece(data.dns_names)
      << "; ip_addrs: " << base::StringPiece(data.ip_addrs);
}

const CertificateNameVerifyTestData kNameVerifyTestData[] = {
    { true, "foo.com", "foo.com" },
    { true, "f", "f" },
    { false, "h", "i" },
    { true, "bar.foo.com", "*.foo.com" },
    { true, "www.test.fr", "common.name",
        "*.test.com,*.test.co.uk,*.test.de,*.test.fr" },
    { true, "wwW.tESt.fr",  "common.name",
        ",*.*,*.test.de,*.test.FR,www" },
    { false, "f.uk", ".uk" },
    { false, "w.bar.foo.com", "?.bar.foo.com" },
    { false, "www.foo.com", "(www|ftp).foo.com" },
    { false, "www.foo.com", "www.foo.com#" },  // # = null char.
    { false, "www.foo.com", "", "www.foo.com#*.foo.com,#,#" },
    { false, "www.house.example", "ww.house.example" },
    { false, "test.org", "", "www.test.org,*.test.org,*.org" },
    { false, "w.bar.foo.com", "w*.bar.foo.com" },
    { false, "www.bar.foo.com", "ww*ww.bar.foo.com" },
    { false, "wwww.bar.foo.com", "ww*ww.bar.foo.com" },
    { false, "wwww.bar.foo.com", "w*w.bar.foo.com" },
    { false, "wwww.bar.foo.com", "w*w.bar.foo.c0m" },
    { false, "WALLY.bar.foo.com", "wa*.bar.foo.com" },
    { false, "wally.bar.foo.com", "*Ly.bar.foo.com" },
    { true, "ww%57.foo.com", "", "www.foo.com" },
    { true, "www&.foo.com", "www%26.foo.com" },
    // Common name must not be used if subject alternative name was provided.
    { false, "www.test.co.jp",  "www.test.co.jp",
        "*.test.de,*.jp,www.test.co.uk,www.*.co.jp" },
    { false, "www.bar.foo.com", "www.bar.foo.com",
      "*.foo.com,*.*.foo.com,*.*.bar.foo.com,*..bar.foo.com," },
    { false, "www.bath.org", "www.bath.org", "", "20.30.40.50" },
    { false, "66.77.88.99", "www.bath.org", "www.bath.org" },
    // IDN tests
    { true, "xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br" },
    { true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    { false, "xn--poema-9qae5a.com.br", "", "*.xn--poema-9qae5a.com.br,"
                                            "xn--poema-*.com.br,"
                                            "xn--*-9qae5a.com.br,"
                                            "*--poema-9qae5a.com.br" },
    // The following are adapted from the  examples quoted from
    // http://tools.ietf.org/html/rfc6125#section-6.4.3
    //  (e.g., *.example.com would match foo.example.com but
    //   not bar.foo.example.com or example.com).
    { true, "foo.example.com", "*.example.com" },
    { false, "bar.foo.example.com", "*.example.com" },
    { false, "example.com", "*.example.com" },
    //   Partial wildcards are disallowed, though RFC 2818 rules allow them.
    //   That is, forms such as baz*.example.net, *baz.example.net, and
    //   b*z.example.net should NOT match domains. Instead, the wildcard must
    //   always be the left-most label, and only a single label.
    { false, "baz1.example.net", "baz*.example.net" },
    { false, "foobaz.example.net", "*baz.example.net" },
    { false, "buzz.example.net", "b*z.example.net" },
    { false, "www.test.example.net", "www.*.example.net" },
    // Wildcards should not be valid for public registry controlled domains,
    // and unknown/unrecognized domains, at least three domain components must
    // be present.
    { true, "www.test.example", "*.test.example" },
    { true, "test.example.co.uk", "*.example.co.uk" },
    { false, "test.example", "*.example" },
    { false, "example.co.uk", "*.co.uk" },
    { false, "foo.com", "*.com" },
    { false, "foo.us", "*.us" },
    { false, "foo", "*" },
    // IDN variants of wildcards and registry controlled domains.
    { true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    { true, "test.example.xn--mgbaam7a8h", "*.example.xn--mgbaam7a8h" },
    { false, "xn--poema-9qae5a.com.br", "*.com.br" },
    { false, "example.xn--mgbaam7a8h", "*.xn--mgbaam7a8h" },
    // Wildcards should be permissible for 'private' registry controlled
    // domains.
    { true, "www.appspot.com", "*.appspot.com" },
    { true, "foo.s3.amazonaws.com", "*.s3.amazonaws.com" },
    // Multiple wildcards are not valid.
    { false, "foo.example.com", "*.*.com" },
    { false, "foo.bar.example.com", "*.bar.*.com" },
    // Absolute vs relative DNS name tests. Although not explicitly specified
    // in RFC 6125, absolute reference names (those ending in a .) should
    // match either absolute or relative presented names.
    { true, "foo.com", "foo.com." },
    { true, "foo.com.", "foo.com" },
    { true, "foo.com.", "foo.com." },
    { true, "f", "f." },
    { true, "f.", "f" },
    { true, "f.", "f." },
    { true, "www-3.bar.foo.com", "*.bar.foo.com." },
    { true, "www-3.bar.foo.com.", "*.bar.foo.com" },
    { true, "www-3.bar.foo.com.", "*.bar.foo.com." },
    { false, ".", "." },
    { false, "example.com", "*.com." },
    { false, "example.com.", "*.com" },
    { false, "example.com.", "*.com." },
    { false, "foo.", "*." },
    { false, "foo", "*." },
    { false, "foo.co.uk", "*.co.uk." },
    { false, "foo.co.uk.", "*.co.uk." },
    // IP addresses in common name; IPv4 only.
    { true, "127.0.0.1", "127.0.0.1" },
    { true, "192.168.1.1", "192.168.1.1" },
    { true,  "676768", "0.10.83.160" },
    { true,  "1.2.3", "1.2.0.3" },
    { false, "192.169.1.1", "192.168.1.1" },
    { false, "12.19.1.1", "12.19.1.1/255.255.255.0" },
    { false, "FEDC:ba98:7654:3210:FEDC:BA98:7654:3210",
      "FEDC:BA98:7654:3210:FEDC:ba98:7654:3210" },
    { false, "1111:2222:3333:4444:5555:6666:7777:8888",
      "1111:2222:3333:4444:5555:6666:7777:8888" },
    { false, "::192.9.5.5", "[::192.9.5.5]" },
    // No wildcard matching in valid IP addresses
    { false, "::192.9.5.5", "*.9.5.5" },
    { false, "2010:836B:4179::836B:4179", "*:836B:4179::836B:4179" },
    { false, "192.168.1.11", "*.168.1.11" },
    { false, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", "*.]" },
    // IP addresses in subject alternative name (common name ignored)
    { true, "10.1.2.3", "", "", "10.1.2.3" },
    { true,  "14.15", "", "", "14.0.0.15" },
    { false, "10.1.2.7", "10.1.2.7", "", "10.1.2.6,10.1.2.8" },
    { false, "10.1.2.8", "10.20.2.8", "foo" },
    { true, "::4.5.6.7", "", "", "x00000000000000000000000004050607" },
    { false, "::6.7.8.9", "::6.7.8.9", "::6.7.8.9",
        "x00000000000000000000000006070808,x0000000000000000000000000607080a,"
        "xff000000000000000000000006070809,6.7.8.9" },
    { true, "FE80::200:f8ff:fe21:67cf", "no.common.name", "",
        "x00000000000000000000000006070808,xfe800000000000000200f8fffe2167cf,"
        "xff0000000000000000000000060708ff,10.0.0.1" },
    // Numeric only hostnames (none of these are considered valid IP addresses).
    { false,  "12345.6", "12345.6" },
    { false, "121.2.3.512", "", "1*1.2.3.512,*1.2.3.512,1*.2.3.512,*.2.3.512",
        "121.2.3.0"},
    { false, "1.2.3.4.5.6", "*.2.3.4.5.6" },
    { true, "1.2.3.4.5", "", "1.2.3.4.5" },
    // Invalid host names.
    { false, "junk)(£)$*!@~#", "junk)(£)$*!@~#" },
    { false, "www.*.com", "www.*.com" },
    { false, "w$w.f.com", "w$w.f.com" },
    { false, "nocolonallowed:example", "", "nocolonallowed:example" },
    { false, "www-1.[::FFFF:129.144.52.38]", "*.[::FFFF:129.144.52.38]" },
    { false, "[::4.5.6.9]", "", "", "x00000000000000000000000004050609" },
};

class X509CertificateNameVerifyTest
    : public testing::TestWithParam<CertificateNameVerifyTestData> {
};

TEST_P(X509CertificateNameVerifyTest, VerifyHostname) {
  CertificateNameVerifyTestData test_data = GetParam();

  std::string common_name(test_data.common_name);
  ASSERT_EQ(std::string::npos, common_name.find(','));
  std::replace(common_name.begin(), common_name.end(), '#', '\0');

  std::vector<std::string> dns_names, ip_addressses;
  if (test_data.dns_names) {
    // Build up the certificate DNS names list.
    std::string dns_name_line(test_data.dns_names);
    std::replace(dns_name_line.begin(), dns_name_line.end(), '#', '\0');
    dns_names = base::SplitString(dns_name_line, ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  }

  if (test_data.ip_addrs) {
    // Build up the certificate IP address list.
    std::string ip_addrs_line(test_data.ip_addrs);
    std::vector<std::string> ip_addressses_ascii = base::SplitString(
        ip_addrs_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (size_t i = 0; i < ip_addressses_ascii.size(); ++i) {
      std::string& addr_ascii = ip_addressses_ascii[i];
      ASSERT_NE(0U, addr_ascii.length());
      if (addr_ascii[0] == 'x') {  // Hex encoded address
        addr_ascii.erase(0, 1);
        std::vector<uint8_t> bytes;
        EXPECT_TRUE(base::HexStringToBytes(addr_ascii, &bytes))
            << "Could not parse hex address " << addr_ascii << " i = " << i;
        ip_addressses.push_back(std::string(reinterpret_cast<char*>(&bytes[0]),
                                            bytes.size()));
        ASSERT_EQ(16U, ip_addressses.back().size()) << i;
      } else {  // Decimal groups
        std::vector<std::string> decimals_ascii = base::SplitString(
            addr_ascii, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
        EXPECT_EQ(4U, decimals_ascii.size()) << i;
        std::string addr_bytes;
        for (size_t j = 0; j < decimals_ascii.size(); ++j) {
          int decimal_value;
          EXPECT_TRUE(base::StringToInt(decimals_ascii[j], &decimal_value));
          EXPECT_GE(decimal_value, 0);
          EXPECT_LE(decimal_value, 255);
          addr_bytes.push_back(static_cast<char>(decimal_value));
        }
        ip_addressses.push_back(addr_bytes);
        ASSERT_EQ(4U, ip_addressses.back().size()) << i;
      }
    }
  }

  bool unused = false;
  EXPECT_EQ(test_data.expected, X509Certificate::VerifyHostname(
      test_data.hostname, common_name, dns_names, ip_addressses, &unused));
}

INSTANTIATE_TEST_CASE_P(, X509CertificateNameVerifyTest,
                        testing::ValuesIn(kNameVerifyTestData));

const struct PublicKeyInfoTestData {
  const char* cert_file;
  size_t expected_bits;
  X509Certificate::PublicKeyType expected_type;
} kPublicKeyInfoTestData[] = {
    {"768-rsa-ee-by-768-rsa-intermediate.pem",
     768,
     X509Certificate::kPublicKeyTypeRSA},
    {"1024-rsa-ee-by-768-rsa-intermediate.pem",
     1024,
     X509Certificate::kPublicKeyTypeRSA},
    {"prime256v1-ecdsa-ee-by-1024-rsa-intermediate.pem",
     256,
     X509Certificate::kPublicKeyTypeECDSA},
#if defined(OS_MACOSX) && !defined(OS_IOS)
    // OS X has an key length limit of 4096 bits. This should manifest as an
    // unknown key. If a future version of OS X changes this, large_key.pem may
    // need to be renegerated with a larger key. See https://crbug.com/472291.
    {"large_key.pem", 0, X509Certificate::kPublicKeyTypeUnknown},
#else
    {"large_key.pem", 8200, X509Certificate::kPublicKeyTypeRSA},
#endif
};

class X509CertificatePublicKeyInfoTest
    : public testing::TestWithParam<PublicKeyInfoTestData> {
};

TEST_P(X509CertificatePublicKeyInfoTest, GetPublicKeyInfo) {
  PublicKeyInfoTestData data = GetParam();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), data.cert_file));
  ASSERT_TRUE(cert.get());

  size_t actual_bits = 0;
  X509Certificate::PublicKeyType actual_type =
      X509Certificate::kPublicKeyTypeUnknown;

  X509Certificate::GetPublicKeyInfo(cert->os_cert_handle(), &actual_bits,
                                    &actual_type);

  EXPECT_EQ(data.expected_bits, actual_bits);
  EXPECT_EQ(data.expected_type, actual_type);
}

INSTANTIATE_TEST_CASE_P(, X509CertificatePublicKeyInfoTest,
                        testing::ValuesIn(kPublicKeyInfoTestData));

}  // namespace net
