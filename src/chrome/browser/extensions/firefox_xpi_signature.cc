// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_xpi_signature.h"

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "extensions/common/firefox_gecko_id.h"
#include "net/cert/x509_certificate.h"

namespace extensions {

namespace {

// AMO's signatures are a few kilobytes. The cap keeps a .xpi from making the
// browser read a large file into memory.
constexpr size_t kMaxSignatureSize = 1024 * 1024;

}  // namespace

std::string GeckoIdFromSignature(const base::FilePath& unzip_dir) {
  std::string signature;
  if (!base::ReadFileToStringWithMaxSize(
          unzip_dir.Append(FILE_PATH_LITERAL("META-INF"))
              .Append(FILE_PATH_LITERAL("mozilla.rsa")),
          &signature, kMaxSignatureSize)) {
    return std::string();
  }
  // The certificates in the PKCS #7, read with BoringSSL's
  // PKCS7_get_raw_certificates(); one that doesn't parse is left out.
  net::CertificateList certificates =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(signature), net::X509Certificate::FORMAT_PKCS7);
  for (const scoped_refptr<net::X509Certificate>& certificate : certificates) {
    const std::string& common_name = certificate->subject().common_name;
    if (!ExtensionIdFromGeckoId(common_name).empty()) {
      return common_name;
    }
  }
  return std::string();
}

}  // namespace extensions
