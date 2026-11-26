// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_VERSION_H_
#define EXTENSIONS_COMMON_FIREFOX_VERSION_H_

#include <string>

#include "base/version.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): the manifest's version
// and name as Firefox loads them. Other extensions keep Chromium's.

// Firefox loads any version string, with a warning when it isn't one to four
// numbers without leading zeros. Chromium refuses a suffix ("3.4resigned1",
// "0.5rc2", "1.3.4buildid20230720.091143"), a leading zero ("01", "099999"),
// more than four numbers ("1.2.3.4.5") or an empty first number (".",
// ".999999999"). For a Firefox add-on (firefox_add_on.h), returns the version
// that the string's leading digits and dots make, an empty number as 0:
// "3.4", "1", "1.2.3.4.5", "0.0", "0.999999999". When that version's string
// isn't the manifest's, the manifest's becomes the add-on's version_name,
// which Chromium shows to the user, unless the manifest has its own. Returns
// an invalid version for any other extension, or if `version` doesn't start
// with a digit or a dot.
base::Version LoadFirefoxVersion(const std::string& version,
                                 Extension* extension);

// Firefox cuts a name longer than 75 UTF-16 code units (JavaScript's
// slice()) to 75. Returns `name` so cut for a Firefox add-on, else `name`.
std::string LoadFirefoxName(const std::string& name,
                            const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_VERSION_H_
