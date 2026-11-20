// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_xpi_signature.h"

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "extensions/browser/zipfile_installer.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/pki/verify.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace extensions {

namespace {

constexpr char kSignatureRootSwitch[] = "firefox-add-ons-signature-root";

// Firefox's cap on a signature file (ReadStream()), so that a .xpi can't make
// the browser read a large file into memory.
constexpr size_t kMaxSignatureFileSize = 8 * 1000 * 1000;

// Mozilla's root for add-ons, the only one Firefox trusts for them
// (AddonsPublicRoot: security/manager/ssl/addons-public.pem in Firefox).
constexpr char kAddonsPublicRoot[] = R"(-----BEGIN CERTIFICATE-----
MIIGZTCCBE2gAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc
MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB
TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et
cHJvZHVjdGlvbi1hbW8wIhgPMjAyNDAyMDEwMDAwMDBaGA8yMjAwMTIwMzAwMDAw
MFowfTELMAkGA1UEBhMCVVMxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24x
LzAtBgNVBAsTJk1vemlsbGEgQU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNl
MR8wHQYDVQQDExZyb290LWNhLXByb2R1Y3Rpb24tYW1vMIICIDANBgkqhkiG9w0B
AQEFAAOCAg0AMIICCAKCAgEAtLth11268Mt+vjD3in+Y4KAEjHzDc+5iVgXnCYL5
U3+IRyWy8zD7CFGrsyPRhKS3x3WesM0shTm+ADPg9ZsQFobmSdzAwCTn9wdUbOkK
Kx65fKqpTbTxqnqZ6TSXC6OybEqqhNzVJu9jIKiB0YE0bKlLPuyyDxnu9utlPjf0
9Cz2FS3uK2dkQx6GmYWGO2vfuPZhziP4NmBQfvcmYxPl3aZU9pAYAOD/HW+4uyNL
SIuG9AdDesvTIS/gkWScsXRXtsAcpCV9eC7IXirDNWsx42Tuekija1vFQCUy5KUc
QprIk69PH5z8gypmnmdAbbLoInHYqXGaV64iBprGWyNdqucrsI0hC3ZA1elGo6Np
/tsLObl6z9+Nl+9VoloQv3ReXc9SyrYwXZlJMWkDDj/7obVxCga2aNaLpw0UMdY+
/kpEOARMIwd0hLa+1w1hjnDiJKCWVjwAnwSwtzyQgVo0wMgZbndAuZ9wsva3oAAJ
ziKiNbdehlNpWkYe3pSe4D0TYEIMC13mXBccOsL8ohHJgozhEOjzPFEI7YTrCpbM
zIZSsL48jhg6M7ZKkgcv7/gLMezltkO65VXymqy9JkRGXUjH6Mt804LTFQjNL4TZ
es3L3+RzTaKaBHYOcrl6NnIWdadSfqvOm9BuciUTHk9vojlWNhEN+7R66lFEZ3As
uLsCAQOjge0wgeowDAYDVR0TBAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwFgYDVR0l
AQH/BAwwCgYIKwYBBQUHAwMwHQYDVR0OBBYEFLO86lh0q+FueCqyq5wjHqhjLJe3
MIGSBgNVHSMEgYowgYehgYGkfzB9MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96
aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlv
biBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1h
bW+CAQEwDQYJKoZIhvcNAQEMBQADggIBAAAJuN23qDiyAZ8y9DhqTfIfdYBXC7jg
f7p5DNCj6mcfLxCMLuSvLljYWm0IfHYs5W0lnQgm8+N0m3SAjQd18AzWfXNklFjZ
1EcP8MRU/keFEk9SmeHZhP916jQfqQvB45w4QwPcJG0nPkWsYzceqD3uL5g2g4Jg
gk+WYWmtE4eeM2BX6cT0itMErYKWei3SF09TdrCX+XpHMixg4nnDdsGe9wxc8F/o
diQ48f/7AZo06moMyZvIH4PPcVt0osAU18fLO0RrVJkupMraxbM1XXL1UwJlyV+p
kvJutX2xBB1f1BA3xPl3NlQneaLIm3JFsw0r7t0z0n1shC6xCi4+t3Fh6Z38CnbS
WwAe5rA2OCQYMjsehxRK9BhmDCG8U65GVd9t8nV4iEJFTrjntBDEFtVD5s4Qnlyv
OUrWd2du4dLCs+WW2E6+R7jZtrsIqFD6qwCLqcgBgC9CM9UgHeUBOixmZLBKCNDE
N1sRkmcVwXcCl5btdgVVq74Mgsd38xsmYuFoMi6nbDLllm6T2ql8LZExyX2i/vo0
pxhEVRaFwj1J1r3TRNXksjdqFcgpNCMf2FRbjDGtVLXRVG0DCCGRayigKgdH78qM
HpdXrbaTDFsfMLTAMnGFnqOZMuMobNJS5M6/vqdepoC8L7xmI5dQgW8YGyymr8DP
gchMof0tylgn
-----END CERTIFICATE-----)";

// AMO's intermediates, which Firefox adds to those in a .xpi, so that an
// add-on whose .xpi has an intermediate that was replaced still installs
// (addons-public-intermediate.pem and addons-public-2018-intermediate.pem).
constexpr char kAddonsPublicIntermediates[] = R"(-----BEGIN CERTIFICATE-----
MIIHMzCCBRugAwIBAgIFFxRnUIAwDQYJKoZIhvcNAQEMBQAwfTELMAkGA1UEBhMC
VVMxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemls
bGEgQU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290
LWNhLXByb2R1Y3Rpb24tYW1vMCIYDzIwMTUwMzE3MDAwMDAwWhgPMjEwMDEyMDMw
MDAwMDBaMIGnMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3Jh
dGlvbjEvMC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNl
cnZpY2UxJjAkBgNVBAMTHXNpZ25pbmdjYTEuYWRkb25zLm1vemlsbGEub3JnMSEw
HwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96aWxsYS5jb20wggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQC/qluiiI+wO6qGA4vH7cHvWvXpdju9JnvbwnrbYmxh
tUpfS68LbdjGGtv7RP6F1XhHT4MU3v4GuMulH0E4Wfalm8evsb3tBJRMJPICJX5U
CLi6VJ6J2vipXSWBf8xbcOB+PY5Kk6L+EZiWaepiM23CdaZjNOJCAB6wFHlGe+zU
k87whpLa7GrtrHjTb8u9TSS+mwjhvgfP8ILZrWhzb5H/ybgmD7jYaJGIDY/WDmq1
gVe03fShxD09Ml1P7H38o5kbFLnbbqpqC6n8SfUI31MiJAXAN2e6rAOM8EmocAY0
EC5KUooXKRsYvHzhwwHkwIbbe6QpTUlIqvw1MPlQPs7Zu/MBnVmyGTSqJxtYoklr
0MaEXnJNY3g3FDf1R0Opp2/BEY9Vh3Fc9Pq6qWIhGoMyWdueoSYa+GURqDbsuYnk
7ZkysxK+yRoFJu4x3TUBmMKM14jQKLgxvuIzWVn6qg6cw7ye/DYNufc+DSPSTSak
SsWJ9IPxiAU7xJ+GCMzaZ10Y3VGOybGLuPxDlSd6KALAoMcl9ghB2mvfB0N3wv6u
WnbKuxihq/qDps+FjliNvr7C66mIVH+9rkyHIy6GgIUlwr7E88Qqw+SQeNeph6NI
Y85PL4p0Y8KivKP4J928tpp18wLuHNbIG+YaUk5WUDZ6/2621pi19UZQ8iiHxN/X
KQIDAQABo4IBiTCCAYUwDAYDVR0TBAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwFgYD
VR0lAQH/BAwwCgYIKwYBBQUHAwMwHQYDVR0OBBYEFBY++xz/DCuT+JsV1y2jwuZ4
YdztMIGoBgNVHSMEgaAwgZ2AFLO86lh0q+FueCqyq5wjHqhjLJe3oYGBpH8wfTEL
MAkGA1UEBhMCVVMxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNV
BAsTJk1vemlsbGEgQU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYD
VQQDExZyb290LWNhLXByb2R1Y3Rpb24tYW1vggEBMDMGCWCGSAGG+EIBBAQmFiRo
dHRwOi8vYWRkb25zLm1vemlsbGEub3JnL2NhL2NybC5wZW0wTgYDVR0eBEcwRaFD
MCCCHi5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzAfgh1jb250ZW50LXNp
Z25hdHVyZS5tb3ppbGxhLm9yZzANBgkqhkiG9w0BAQwFAAOCAgEAKTTFmoGTNjPd
T7NruchQNxokV/lT19bfdXvGVGBmYeWdCCOWshLzvxAJ9XVqbi1Nrl4CexJGts6p
RTPoJFgwZfbv6YKo1rCgbTB0bRBjucEOEwwv2hkRetdZRAdKon2wxvwbB3h8V54Z
4D+zCETq/+8rmEveGdMaO4b2LNgzToxlki6KXF/DGUYjzsB7yjEcAYvQGsZMkaeS
wNc2aT/Adj2mqbWpS5R/eA+yhv8KW1etAUzyaTUXyL3KrXyDXJJ5duMdfomQqqL6
EeGTxR9iTVqZUNfqw+3/WgJ3Wz2atRFRpkhM67KBJhj6fsWr84omUojaLs8qM8jz
lxMjBVZP0QnWta5tc5zmrQFSuAvqDEJ9zI/CoP08OsHyWrTHcj9H5OIH5zCCJoj5
MGTgnWcAoiPpIF527tlh8y616YlfNY4d8njKUoTbYzQMmIM7cHIAmXUCbV+tlNiK
TrhgxEWfWxWU1Z7HyQH2OZkXLc/Ub5zZctQLeZ4/6zeMwkYBIde04F3ogs6W98ea
x7ScfMRQs+v/CBY1zmdd+QjJXuIT2omFxRLIQF/qNr9dLhQ/1Zx0nJj56F3IxaD0
VWFT3nrzrFxp9KgcYdnv60Tt9J5LQBc8LaI+8UoKikuW+HfFX3F6li0mXTdaUtuo
b7w2V9cDKpiO0A9WD/Fape1mMI9L6YY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIHUTCCBTmgAwIBAgIFF0I0ByAwDQYJKoZIhvcNAQEMBQAwfTELMAkGA1UEBhMC
VVMxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemls
bGEgQU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290
LWNhLXByb2R1Y3Rpb24tYW1vMCIYDzIwMTUwMzE3MDAwMDAwWhgPMjEyNTAzMTkw
MDAwMDBaMIHFMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3Jh
dGlvbjEvMC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNl
cnZpY2UxMTAvBgNVBAMTKHByb2R1Y3Rpb24tc2lnbmluZy1jYS5hZGRvbnMubW96
aWxsYS5vcmcxNDAyBgkqhkiG9w0BCQEWJXNlcnZpY2VzLW9wcythZGRvbnNpZ25p
bmdAbW96aWxsYS5jb20wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDC
zDPZthwS4QojvZoZY6XoU4cac5Qp8Zbsw2edSPx+zLwWyL2QZvc0qR3X37m0rGpN
qmBOvXfnm02vwivN1iKLDNuP9kKe5/xhmjyXAguGNyZKwrIAPPsFdjGxZSkr/qjH
byP7BNwT2AzAo1TzIR5iD12j1neNjqB6SPEdxhlLVZ0dx5u+qWtvr+k8IHjIjvKJ
vqIBrn4sr+hTTGeTbmNWfWizRL3oZwM0n3XUMpIvuhSXe15IGWz+6W9SpfMmIIsu
KYgXqyfR9Pbj3oX3xNxJZd3AKUC9/tJI2lF+LHDI7jUAzCs4kWZGICQDPqgPVgOS
Z++wI3q0AVkHhHr9T1h2IdCA5YEv2tG4yplYzWWBumna2P0ef8Ic1hhQiBNVH4T5
uj3TdOee6rxfN/AtkRR7CCs1eikH9HITI6WtNrT2iHLY1YZw0GWTSptmXyzrfZEO
eyLP3uisR+6WDIreOvrPfZWDO2arr1mytfW9SJYv+ktQcUs3d8M7YHzeYxrHPowg
mC5vgHFbXzSnPHBWR1yqwQsIRpnW1g0gQonGTQOxSiaTt6CMdulTxdl1JXQP0+TI
fzzhi4pSt7fZvam4xMg6dRMKAU6Dkf2jK3ZzWL0lqdIPxhzM0nyY418gKq2WNorW
pvyCXfK/RaEQM3rz7f801wRE9yDaIggxkR9F68VGLQIDBlU3o4IBiTCCAYUwDAYD
VR0TBAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwFgYDVR0lAQH/BAwwCgYIKwYBBQUH
AwMwHQYDVR0OBBYEFHR8XxSl4qWS47NtBqiFHTZEFx+IMIGoBgNVHSMEgaAwgZ2A
FLO86lh0q+FueCqyq5wjHqhjLJe3oYGBpH8wfTELMAkGA1UEBhMCVVMxHDAaBgNV
BAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEgQU1PIFBy
b2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNhLXByb2R1
Y3Rpb24tYW1vggEBMDMGCWCGSAGG+EIBBAQmFiRodHRwOi8vYWRkb25zLm1vemls
bGEub3JnL2NhL2NybC5wZW0wTgYDVR0eBEcwRaFDMCCCHi5jb250ZW50LXNpZ25h
dHVyZS5tb3ppbGxhLm9yZzAfgh1jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9y
ZzANBgkqhkiG9w0BAQwFAAOCAgEAEFaxVq63OzCyakgVbjKZiUyUtc6913O+TLBO
DUdst3XQiOia5WJc2cN9FhXPe2kVBSxChBlPwljOt+3hUqbAm7++W5FD3IE+SYcq
B4wVmdXGtxyMrOobrYZV2JqHcQF2Co/ANV2yFYCFTwBMz9Gzo+dK50nRIBK7LK6c
i5z2F9XOEbKRTTPZKceePdo/HpNNdECF+qGNXDWZN++B/739NQJ1HoDQMIDUu6jD
+o0gxjuawYn1Jutylz7FTX6781uu88YFkvyDxPHxCLi6W3D7XfFmJ9a1TVs+FyRm
5THpB3NABudI/mKCwGDuKnd/nEywORC5ndNI0ADrTG5PAPFYRUIzvicrOqjP+03H
MJ4Wnuc8tN7HSG/25OY6srCF3CmetF/QepCPQFNRGBXpk+ULDPemcXj1671Mc76q
iKLe8GrQLEPN7bLL4M0XDTwJE55sBLsvX/5XdHDIYmHDcXtY3NH3BBxBTZH4EZYp
QEBvc7chKTh7oIXWNRl8rXpEFqlWk3aohwUe5yb6qLTroznWprM49O0u1y4PdzGJ
HWwvV/byCBGJTQD3gNyTA/6CYz+ixzCCGW08+SmLaOWz2Vvx5BH7m9Tp7+KXKnta
w74obtG7+d02oqYuCXF5PcadlntXmjhAQIBXVw7mhm8A3nVZv+SzsbbqLuFFqXBt
LWcdh6Y=
-----END CERTIFICATE-----)";

// DER object identifiers of PKCS #7 and its attributes.
constexpr uint8_t kSignedDataOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                      0x0d, 0x01, 0x07, 0x02};
constexpr uint8_t kDataOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                0x0d, 0x01, 0x07, 0x01};
constexpr uint8_t kContentTypeOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                       0x0d, 0x01, 0x09, 0x03};
constexpr uint8_t kMessageDigestOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                         0x0d, 0x01, 0x09, 0x04};

constexpr CBS_ASN1_TAG kTag0 = CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED;
constexpr CBS_ASN1_TAG kTag1 =
    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1;

// The names that Firefox finds manifest.mf by:
// "(M|/M)ETA-INF/(M|m)(ANIFEST|anifest).(MF|mf)$".
constexpr std::string_view kManifestNames[] = {
    "META-INF/MANIFEST.MF", "META-INF/MANIFEST.mf", "META-INF/Manifest.MF",
    "META-INF/Manifest.mf", "META-INF/mANIFEST.MF", "META-INF/mANIFEST.mf",
    "META-INF/manifest.MF", "META-INF/manifest.mf"};

CBS CbsOf(std::string_view data) {
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(data.data()), data.size());
  return cbs;
}

std::string_view StringOf(const CBS& cbs) {
  // SAFETY: CBS_data() points to CBS_len() bytes.
  return UNSAFE_BUFFERS(std::string_view(
      reinterpret_cast<const char*>(CBS_data(&cbs)), CBS_len(&cbs)));
}

// The DER certificates in `data`, PEM or DER.
std::vector<std::string> DerCertificates(std::string_view data) {
  std::vector<std::string> der;
  for (const scoped_refptr<net::X509Certificate>& certificate :
       net::X509Certificate::CreateCertificateListFromBytes(
           base::as_byte_span(data), net::X509Certificate::FORMAT_AUTO)) {
    der.emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()));
  }
  return der;
}

bssl::UniquePtr<X509> ParseCertificate(std::string_view der) {
  bssl::UniquePtr<CRYPTO_BUFFER> buffer =
      net::x509_util::CreateCryptoBuffer(der);
  return bssl::UniquePtr<X509>(X509_parse_from_buffer(buffer.get()));
}

// The digest names of the JAR format: Firefox's "sha256-digest-manifest" and
// "sha256-digest", or SHA-1's.
std::string DigestName(const EVP_MD* md) {
  return md == EVP_sha256() ? "sha256" : "sha1";
}

std::string Digest(const EVP_MD* md, std::string_view data) {
  std::array<uint8_t, EVP_MAX_MD_SIZE> digest;
  unsigned int size = 0;
  EVP_Digest(data.data(), data.size(), digest.data(), &size, md, nullptr);
  return std::string(base::as_string_view(base::span(digest).first(size)));
}

// The digest of the file at `path`, read in parts, as Firefox does
// (VerifyStreamContentDigest()), since an add-on's file can be large.
std::optional<std::string> FileDigest(const EVP_MD* md,
                                      const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }
  bssl::ScopedEVP_MD_CTX context;
  EVP_DigestInit_ex(context.get(), md, nullptr);
  std::vector<uint8_t> buffer(128 * 1024);
  while (true) {
    std::optional<size_t> read = file.ReadAtCurrentPos(buffer);
    if (!read) {
      return std::nullopt;
    }
    if (*read == 0) {
      break;
    }
    EVP_DigestUpdate(context.get(), buffer.data(), *read);
  }
  std::array<uint8_t, EVP_MAX_MD_SIZE> digest;
  unsigned int size = 0;
  EVP_DigestFinal_ex(context.get(), digest.data(), &size);
  return std::string(base::as_string_view(base::span(digest).first(size)));
}

// The files in `dir`, with their paths in the archive. A directory isn't one,
// as Firefox doesn't check an archive's entries for directories.
std::set<std::string> FilesIn(const base::FilePath& dir) {
  std::set<std::string> files;
  base::FileEnumerator enumerator(dir, /*recursive=*/true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::FilePath relative;
    if (dir.AppendRelativePath(path, &relative)) {
      files.insert(relative.NormalizePathSeparatorsTo(FILE_PATH_LITERAL('/'))
                       .AsUTF8Unsafe());
    }
  }
  return files;
}

// The one file of `files` in META-INF/ that ends in `extension`, in lower or
// upper case, as Firefox's FindAndLoadOneEntry() finds the signature files,
// for example by "(M|/M)ETA-INF/*.(RSA|rsa)$", or "" when there is none or
// more than one.
std::string OneSignatureFile(const std::set<std::string>& files,
                             std::string_view extension,
                             std::string_view upper_case_extension) {
  std::string found;
  for (const std::string& file : files) {
    if (base::StartsWith(file, "META-INF/") &&
        (base::EndsWith(file, extension) ||
         base::EndsWith(file, upper_case_extension))) {
      if (!found.empty()) {
        return std::string();
      }
      found = file;
    }
  }
  return found;
}

std::string OneManifestFile(const std::set<std::string>& files) {
  std::string found;
  for (std::string_view name : kManifestNames) {
    if (files.contains(std::string(name))) {
      if (!found.empty()) {
        return std::string();
      }
      found = name;
    }
  }
  return found;
}

// Firefox's ReadLine(): the next line of a JAR manifest or signature file
// from `rest`, with the lines that continue it, which start with a space,
// joined to it. False for a line longer than the JAR format's 72 bytes.
bool ReadLine(std::string_view& rest,
              std::string& line,
              bool allow_continuations = true) {
  line.clear();
  while (true) {
    size_t end = rest.find_first_of("\r\n");
    if (end == std::string_view::npos) {
      end = rest.size();
    }
    if (end > 72) {
      return false;
    }
    line.append(rest.substr(0, end));
    if (line.size() > 65535) {
      return false;
    }
    rest.remove_prefix(end);
    if (rest.starts_with('\r')) {
      rest.remove_prefix(1);
    }
    if (rest.starts_with('\n')) {
      rest.remove_prefix(1);
    }
    if (!rest.starts_with(' ')) {
      return true;
    }
    if (!allow_continuations) {
      return false;
    }
    rest.remove_prefix(1);
  }
}

// Firefox's ParseAttribute(): a line's "name: value", without the spaces
// around the colon.
bool ParseAttribute(std::string_view line,
                    std::string_view& name,
                    std::string_view& value) {
  size_t colon = line.find(':');
  if (colon == std::string_view::npos) {
    return false;
  }
  const std::string_view before = line.substr(0, colon);
  const std::string_view after = line.substr(colon + 1);
  name = base::TrimString(before, " ", base::TRIM_TRAILING);
  value = base::TrimString(after, " ", base::TRIM_LEADING);
  return !name.empty();
}

// Firefox's ParseSF(): the digest of manifest.mf that `sf`, mozilla.sf, gives
// in its main section for `md`.
std::optional<std::string> ManifestDigest(std::string_view sf,
                                          const EVP_MD* md) {
  std::string line;
  if (!ReadLine(sf, line, /*allow_continuations=*/false) ||
      line != "Signature-Version: 1.0") {
    return std::nullopt;
  }
  const std::string wanted = DigestName(md) + "-digest-manifest";
  while (true) {
    std::string_view name, value;
    if (!ReadLine(sf, line) || line.empty() ||
        !ParseAttribute(line, name, value)) {
      return std::nullopt;
    }
    if (base::EqualsCaseInsensitiveASCII(name, wanted)) {
      std::string digest;
      if (!base::Base64Decode(value, &digest)) {
        return std::nullopt;
      }
      return digest;
    }
  }
}

// Whether the file `name` of a manifest.mf section has the section's
// `digest`. A file that ZipFileInstaller didn't unzip is in the .xpi, where
// Firefox finds it, so its digest goes unchecked; any other that isn't in
// `files` is missing, which fails in Firefox too.
bool HasDigest(const base::FilePath& unzip_dir,
               const std::set<std::string>& files,
               bool is_theme,
               const EVP_MD* md,
               const std::string& name,
               const std::string& digest) {
  const base::FilePath path = base::FilePath::FromUTF8Unsafe(name);
  if (!files.contains(name)) {
    return !ZipFileInstaller::ShouldExtractFile(is_theme, path);
  }
  return digest.size() == EVP_MD_size(md) &&
         FileDigest(md, unzip_dir.Append(path)) == digest;
}

// Firefox's ParseMF() and VerifyAppManifest(): whether `mf`, manifest.mf,
// lists every file of `files` once, each with its `md` digest, in sections
// of a "Name" and a digest, and no file that is missing.
bool VerifyManifest(std::string_view mf,
                    const EVP_MD* md,
                    const base::FilePath& unzip_dir,
                    const std::set<std::string>& files,
                    bool is_theme) {
  std::string line;
  if (!ReadLine(mf, line, /*allow_continuations=*/false) ||
      line != "Manifest-Version: 1.0") {
    return false;
  }
  // The rest of the main section.
  do {
    if (!ReadLine(mf, line)) {
      return false;
    }
  } while (!line.empty());
  const std::string digest_name = DigestName(md) + "-digest";
  std::set<std::string> listed;
  std::string name, digest;
  while (!mf.empty()) {
    if (!ReadLine(mf, line)) {
      return false;
    }
    if (line.empty()) {
      // The end of a section, which has to have both, for a file not listed
      // before.
      if (name.empty() || digest.empty() || listed.contains(name) ||
          !HasDigest(unzip_dir, files, is_theme, md, name, digest)) {
        return false;
      }
      listed.insert(name);
      name.clear();
      digest.clear();
      continue;
    }
    std::string_view attribute, value;
    if (!ParseAttribute(line, attribute, value)) {
      return false;
    }
    if (base::EqualsCaseInsensitiveASCII(attribute, digest_name)) {
      if (!digest.empty() || !base::Base64Decode(value, &digest)) {
        return false;
      }
    } else if (base::EqualsCaseInsensitiveASCII(attribute, "name")) {
      if (!name.empty() || value.empty()) {
        return false;
      }
      name = value;
    } else if (base::EqualsCaseInsensitiveASCII(attribute, "magic")) {
      // Firefox understands no "Magic", so it can't check the file.
      return false;
    }
  }
  // The last section, which ends with the file.
  if (!name.empty() || !digest.empty()) {
    if (name.empty() || digest.empty() || listed.contains(name) ||
        !HasDigest(unzip_dir, files, is_theme, md, name, digest)) {
      return false;
    }
    listed.insert(name);
  }
  for (const std::string& file : files) {
    if (!listed.contains(file)) {
      return false;
    }
  }
  return true;
}

// A PKCS #7 SignerInfo, with what Firefox reads of it.
struct Signer {
  // SHA-256 or SHA-1, the digests Firefox takes.
  const EVP_MD* md = nullptr;
  // Whether it names its certificate by issuer and serial number, as AMO's
  // do, the only way Firefox finds the certificate.
  bool named = false;
  CBS issuer = {};
  CBS serial = {};
  // Its signed attributes as their DER [0], if it has them.
  std::optional<std::string> attributes;
  CBS signature = {};
};

// The issuer and serial number of the DER certificate `der`.
bool GetIssuerAndSerial(std::string_view der, CBS* issuer, CBS* serial) {
  CBS in = CbsOf(der), certificate, tbs;
  return CBS_get_asn1(&in, &certificate, CBS_ASN1_SEQUENCE) &&
         CBS_get_asn1(&certificate, &tbs, CBS_ASN1_SEQUENCE) &&
         CBS_get_optional_asn1(&tbs, nullptr, nullptr, kTag0) &&
         CBS_get_asn1(&tbs, serial, CBS_ASN1_INTEGER) &&
         CBS_get_asn1(&tbs, nullptr, CBS_ASN1_SEQUENCE) &&
         CBS_get_asn1_element(&tbs, issuer, CBS_ASN1_SEQUENCE);
}

// Parses the PKCS #7 `rsa` into its `certificates` and, as Firefox picks it
// (GetAllSignerInfosForSupportedDigestAlgorithms()), its first signer with a
// SHA-256 digest, or else with SHA-1.
bool ParsePkcs7(std::string_view rsa,
                std::vector<std::string_view>* certificates,
                Signer* signer) {
  CBS in = CbsOf(rsa), content_info, type, wrapper, signed_data, infos;
  CBS certs = {};
  if (!CBS_get_asn1(&in, &content_info, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&content_info, &type, CBS_ASN1_OBJECT) ||
      !CBS_mem_equal(&type, kSignedDataOid, sizeof(kSignedDataOid)) ||
      !CBS_get_asn1(&content_info, &wrapper, kTag0) ||
      !CBS_get_asn1(&wrapper, &signed_data, CBS_ASN1_SEQUENCE) ||
      // The version, the digest algorithms and the content's type.
      !CBS_get_asn1(&signed_data, nullptr, CBS_ASN1_INTEGER) ||
      !CBS_get_asn1(&signed_data, nullptr, CBS_ASN1_SET) ||
      !CBS_get_asn1(&signed_data, nullptr, CBS_ASN1_SEQUENCE) ||
      !CBS_get_optional_asn1(&signed_data, &certs, nullptr, kTag0) ||
      // The CRLs.
      !CBS_get_optional_asn1(&signed_data, nullptr, nullptr, kTag1) ||
      !CBS_get_asn1(&signed_data, &infos, CBS_ASN1_SET)) {
    return false;
  }
  while (CBS_len(&certs) > 0) {
    CBS certificate;
    if (!CBS_get_asn1_element(&certs, &certificate, CBS_ASN1_SEQUENCE)) {
      return false;
    }
    certificates->push_back(StringOf(certificate));
  }
  std::optional<Signer> sha1_signer;
  while (CBS_len(&infos) > 0) {
    Signer candidate;
    CBS info, sid, algorithm, oid;
    CBS_ASN1_TAG sid_tag;
    if (!CBS_get_asn1(&infos, &info, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&info, nullptr, CBS_ASN1_INTEGER) ||
        !CBS_get_any_asn1(&info, &sid, &sid_tag) ||
        !CBS_get_asn1(&info, &algorithm, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&algorithm, &oid, CBS_ASN1_OBJECT)) {
      return false;
    }
    if (CBS_peek_asn1_tag(&info, kTag0)) {
      CBS attributes;
      if (!CBS_get_asn1_element(&info, &attributes, kTag0)) {
        return false;
      }
      candidate.attributes = std::string(StringOf(attributes));
    }
    if (!CBS_get_asn1(&info, nullptr, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&info, &candidate.signature, CBS_ASN1_OCTETSTRING)) {
      return false;
    }
    candidate.named =
        sid_tag == CBS_ASN1_SEQUENCE &&
        CBS_get_asn1_element(&sid, &candidate.issuer, CBS_ASN1_SEQUENCE) &&
        CBS_get_asn1(&sid, &candidate.serial, CBS_ASN1_INTEGER);
    // Firefox reads only the digest algorithm's OID.
    const int nid = OBJ_cbs2nid(&oid);
    if (nid == NID_sha256) {
      candidate.md = EVP_sha256();
      *signer = candidate;
      return true;
    }
    if (nid == NID_sha1 && !sha1_signer) {
      candidate.md = EVP_sha1();
      sha1_signer = candidate;
    }
  }
  if (!sha1_signer) {
    return false;
  }
  *signer = *sha1_signer;
  return true;
}

// Firefox's VerifyCertificate(): whether the DER certificate `signer`, with
// the `certificates` of the PKCS #7 as intermediates, has a chain to the
// add-ons root or the switch's, for signing code. Firefox ignores the
// certificates' validity periods, so that an add-on keeps working once its
// certificate expires: the chain is checked at the time the signing
// certificate starts to be valid, when AMO signed with it.
bool VerifyChain(std::string_view signer,
                 const std::vector<std::string_view>& certificates) {
  scoped_refptr<net::X509Certificate> parsed =
      net::X509Certificate::CreateFromBytes(base::as_byte_span(signer));
  if (!parsed) {
    return false;
  }
  std::vector<std::string> roots = DerCertificates(kAddonsPublicRoot);
  const base::FilePath root_file =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSignatureRootSwitch);
  std::string root;
  if (!root_file.empty() && base::ReadFileToString(root_file, &root)) {
    for (std::string& der : DerCertificates(root)) {
      roots.push_back(std::move(der));
    }
  }
  std::string diagnostic;
  std::unique_ptr<bssl::VerifyTrustStore> trust_store =
      bssl::VerifyTrustStore::FromDER(
          std::vector<std::string_view>(roots.begin(), roots.end()),
          &diagnostic);
  if (!trust_store) {
    return false;
  }
  const std::vector<std::string> amo_intermediates =
      DerCertificates(kAddonsPublicIntermediates);
  bssl::CertificateVerifyOptions options;
  // Firefox's code signing purpose, which the path builder doesn't have, is
  // checked below.
  options.key_purpose = bssl::CertificateVerifyOptions::KeyPurpose::ANY_EKU;
  options.leaf_cert = signer;
  options.intermediates = certificates;
  options.intermediates.insert(options.intermediates.end(),
                               amo_intermediates.begin(),
                               amo_intermediates.end());
  options.trust_store = trust_store.get();
  // Firefox's AppTrustDomain: RSA keys of 2048 bits at least, and no SHA-1
  // in certificates' signatures, which the path builder refuses by default.
  options.min_rsa_modulus_length = 2048;
  options.time = parsed->valid_start().ToTimeT();
  std::optional<std::vector<std::string>> chain =
      bssl::CertificateVerify(options);
  if (!chain) {
    return false;
  }
  // mozilla::pkix's checks for KeyPurposeId::id_kp_codeSigning and
  // KeyUsage::digitalSignature: an extended key usage, where a certificate
  // has one, has code signing, and the signing certificate, not a CA, may
  // sign, where it has a key usage.
  for (size_t i = 0; i < chain->size(); ++i) {
    bssl::UniquePtr<X509> certificate = ParseCertificate((*chain)[i]);
    if (!certificate ||
        !(X509_get_extended_key_usage(certificate.get()) & XKU_CODE_SIGN)) {
      return false;
    }
    if (i == 0 && (!(X509_get_key_usage(certificate.get()) &
                     X509v3_KU_DIGITAL_SIGNATURE) ||
                   (X509_get_extension_flags(certificate.get()) & EXFLAG_CA))) {
      return false;
    }
  }
  return true;
}

// The one value of the attribute `oid` in `attributes`, which is a `tag`, or
// false when the attribute is missing, given twice, or has more than one
// value, as NSS finds it (NSS_CMSAttributeArray_FindAttrByOidTag() and
// NSS_CMSAttribute_GetValue()).
bool GetAttribute(CBS attributes,
                  base::span<const uint8_t> oid,
                  CBS_ASN1_TAG tag,
                  CBS* value) {
  bool found = false;
  while (CBS_len(&attributes) > 0) {
    CBS attribute, type, values;
    if (!CBS_get_asn1(&attributes, &attribute, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&attribute, &type, CBS_ASN1_OBJECT) ||
        !CBS_get_asn1(&attribute, &values, CBS_ASN1_SET)) {
      return false;
    }
    if (!CBS_mem_equal(&type, oid.data(), oid.size())) {
      continue;
    }
    if (found || !CBS_get_asn1(&values, value, tag) || CBS_len(&values) > 0) {
      return false;
    }
    found = true;
  }
  return found;
}

// NSS's NSS_CMSSignerInfo_Verify(), as Firefox calls it: whether `signer`,
// with the DER certificate `certificate`, signed `sf`. With signed
// attributes, as AMO's signers have, they give the content's type, data, and
// its digest, and the signature is of their DER as a SET; without, it is of
// `sf` itself.
bool VerifySignature(const Signer& signer,
                     std::string_view certificate,
                     std::string_view sf) {
  std::string signed_data(sf);
  if (signer.attributes) {
    CBS element = CbsOf(*signer.attributes), attributes, type, digest;
    if (!CBS_get_asn1(&element, &attributes, kTag0) ||
        !GetAttribute(attributes, kContentTypeOid, CBS_ASN1_OBJECT, &type) ||
        !CBS_mem_equal(&type, kDataOid, sizeof(kDataOid)) ||
        !GetAttribute(attributes, kMessageDigestOid, CBS_ASN1_OCTETSTRING,
                      &digest) ||
        StringOf(digest) != Digest(signer.md, sf)) {
      return false;
    }
    // Only the tag differs, one byte in both: [0] becomes a SET.
    signed_data = *signer.attributes;
    signed_data[0] = 0x31;
  }
  bssl::UniquePtr<X509> parsed = ParseCertificate(certificate);
  EVP_PKEY* key = parsed ? X509_get0_pubkey(parsed.get()) : nullptr;
  bssl::ScopedEVP_MD_CTX context;
  return key &&
         EVP_DigestVerifyInit(context.get(), nullptr, signer.md, nullptr,
                              key) &&
         EVP_DigestVerify(context.get(), CBS_data(&signer.signature),
                          CBS_len(&signer.signature),
                          reinterpret_cast<const uint8_t*>(signed_data.data()),
                          signed_data.size());
}

bool ReadSignatureFile(const base::FilePath& unzip_dir,
                       const std::string& name,
                       std::string* contents) {
  return base::ReadFileToStringWithMaxSize(
      unzip_dir.Append(base::FilePath::FromUTF8Unsafe(name)), contents,
      kMaxSignatureFileSize);
}

}  // namespace

XpiSignature CheckXpiSignature(const base::FilePath& unzip_dir,
                               bool is_theme,
                               std::string* signer_id) {
  std::set<std::string> files = FilesIn(unzip_dir);
  const std::string rsa_name = OneSignatureFile(files, ".rsa", ".RSA");
  if (rsa_name.empty()) {
    return XpiSignature::kMissing;
  }
  const std::string sf_name = OneSignatureFile(files, ".sf", ".SF");
  const std::string mf_name = OneManifestFile(files);
  std::string rsa, sf, mf;
  if (sf_name.empty() || mf_name.empty() ||
      !ReadSignatureFile(unzip_dir, rsa_name, &rsa) ||
      !ReadSignatureFile(unzip_dir, sf_name, &sf) ||
      !ReadSignatureFile(unzip_dir, mf_name, &mf)) {
    return XpiSignature::kInvalid;
  }
  // The signature's own files, which no digest covers. Firefox also checks
  // META-INF/cose.sig, a COSE signature, when a .xpi has one; to this it is
  // a file like the others, whose digest manifest.mf gives.
  files.erase(rsa_name);
  files.erase(sf_name);
  files.erase(mf_name);

  std::vector<std::string_view> certificates;
  Signer signer;
  if (!ParsePkcs7(rsa, &certificates, &signer) || !signer.named) {
    return XpiSignature::kInvalid;
  }
  std::string_view signer_certificate;
  for (std::string_view certificate : certificates) {
    CBS issuer, serial;
    if (GetIssuerAndSerial(certificate, &issuer, &serial) &&
        StringOf(issuer) == StringOf(signer.issuer) &&
        StringOf(serial) == StringOf(signer.serial)) {
      signer_certificate = certificate;
      break;
    }
  }
  if (signer_certificate.empty() ||
      !VerifyChain(signer_certificate, certificates) ||
      !VerifySignature(signer, signer_certificate, sf)) {
    return XpiSignature::kInvalid;
  }
  std::optional<std::string> mf_digest = ManifestDigest(sf, signer.md);
  if (mf_digest != Digest(signer.md, mf) ||
      !VerifyManifest(mf, signer.md, unzip_dir, files, is_theme)) {
    return XpiSignature::kInvalid;
  }
  scoped_refptr<net::X509Certificate> parsed =
      net::X509Certificate::CreateFromBytes(
          base::as_byte_span(signer_certificate));
  if (!parsed) {
    return XpiSignature::kInvalid;
  }
  *signer_id = parsed->subject().common_name;
  return XpiSignature::kValid;
}

}  // namespace extensions
