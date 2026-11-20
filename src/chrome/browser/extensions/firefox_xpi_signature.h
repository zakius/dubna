// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_

#include <string>

namespace base {
class FilePath;
}

namespace extensions {

// What CheckXpiSignature() finds, as Firefox tells the cases apart.
enum class XpiSignature {
  // Signed by AMO, or by the root that the switch names.
  kValid,
  // Without a META-INF/*.rsa: Firefox says the add-on "has not been
  // verified".
  kMissing,
  // Anything else that fails: Firefox says it "appears to be corrupt".
  kInvalid,
};

// Firefox add-ons (dubna's webextensions/README.md): checks the AMO signature
// of the .xpi unzipped in `unzip_dir` as Firefox checks a .xpi before it
// installs it, with its default policy (AppSignatureVerification.cpp,
// AppTrustDomain.cpp). META-INF/mozilla.rsa is a PKCS #7 signature of
// META-INF/mozilla.sf, SHA-256 or SHA-1, by a certificate for code signing
// whose chain ends at Mozilla's add-ons root; mozilla.sf gives the digest of
// META-INF/manifest.mf, which gives the digest of every other file, and every
// file is listed. On kValid, `signer_id` is the signing certificate's common
// name, which is the add-on's ID. The command-line switch
// --firefox-add-ons-signature-root=<PEM or DER file> trusts one more root, as
// Firefox's xpinstall.signatures.dev-root pref picks another one, for tests.
// `is_theme` says whether the add-on is a theme, of which ZipFileInstaller
// unzips fewer files. Reads files, so it runs where blocking is allowed.
XpiSignature CheckXpiSignature(const base::FilePath& unzip_dir,
                               bool is_theme,
                               std::string* signer_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_
