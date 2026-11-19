// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_

#include <string>

namespace base {
class FilePath;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the add-on's ID in the
// AMO signature of the .xpi unzipped in `unzip_dir`, or "". AMO signs a .xpi
// in META-INF/mozilla.rsa, a PKCS #7 signature whose signing certificate has
// the add-on's ID as its common name, and Firefox takes the ID from there
// when the manifest has no Gecko ID. The first common name that is an ID
// Firefox takes is the add-on's: AMO's own certificate has a host name.
// Unlike Firefox, this doesn't check the signature: a .xpi can claim any ID
// with a Gecko ID in its manifest already, so the ID is trusted no more than
// that one. Reads a file, so it runs where blocking is allowed.
std::string GeckoIdFromSignature(const base::FilePath& unzip_dir);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_XPI_SIGNATURE_H_
