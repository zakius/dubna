// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_
#define EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_

#include <string>
#include <vector>

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox takes the host
// patterns in a Manifest V3 add-on's permissions as host permissions, and
// those in its optional_permissions as optional ones, as in Manifest V2
// (permissionsObject() in Extension.sys.mjs). Chromium reads Manifest V3's
// host patterns only from host_permissions and optional_host_permissions.
// For a Firefox add-on (firefox_add_on.h), moves the host patterns out of
// `unhandled`, the strings of such a key that aren't API permissions, and
// returns them. A string that Firefox reads as a permission's name ("foo",
// "foo.bar") stays. Returns none for other extensions.
std::vector<std::string> TakeFirefoxHostPatterns(
    const Extension& extension,
    std::vector<std::string>* unhandled);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_
