// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_VERSION_H_
#define EXTENSIONS_COMMON_FIREFOX_VERSION_H_

#include <string>

#include "base/version.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox takes a version
// with a suffix, which Chromium refuses: "3.4resigned1" (Mozilla's re-signed
// builds), "0.5rc2", "1.3.4buildid20230720.091143". Returns the leading
// numbers ("3.4") as the version, and sets the full string as the add-on's
// version_name, which Chromium shows to the user, unless the manifest has its
// own. Returns an invalid version if `version` doesn't start with one to four
// numbers.
base::Version LoadFirefoxVersion(const std::string& version,
                                 Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_VERSION_H_
