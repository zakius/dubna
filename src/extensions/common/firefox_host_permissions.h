// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_
#define EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_

#include <string>
#include <string_view>
#include <vector>

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox takes the host
// patterns in an add-on's optional_permissions as optional host permissions
// in Manifest V3 too (OptionalPermissionOrOrigin in its manifest schema).
// Chromium reads Manifest V3's host patterns only from host_permissions and
// optional_host_permissions. A Manifest V3 add-on's permissions take only
// permission names in Firefox, which drops a host pattern there with a
// warning, as Chromium does.
// For a Firefox add-on (firefox_add_on.h) and the key optional_permissions,
// moves the host patterns out of `unhandled`, the key's strings that aren't
// API permissions, and returns them. A string that Firefox reads as a
// permission's name ("foo", "foo.bar") stays. Returns none for other keys
// and other extensions.
std::vector<std::string> TakeFirefoxHostPatterns(
    const Extension& extension,
    std::string_view key,
    std::vector<std::string>* unhandled);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_HOST_PERMISSIONS_H_
