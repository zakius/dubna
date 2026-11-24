// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_
#define EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md, "Firefox add-ons only"):
// whether `extension` gets Firefox's behaviour where Firefox and Chromium
// differ. Every other extension keeps Chromium's, since a Chrome Web Store
// extension may tell Chrome from Firefox by that behaviour.
//
// - Installed from a .xpi: always. The .xpi installer gives the add-on the ID
//   that its Gecko ID makes (firefox_gecko_id.h), and writes a Gecko ID from
//   the AMO signature into a manifest without one.
// - Installed from a .crx, the Chrome Web Store's or any other: never, even
//   with a Gecko ID in its manifest, as an extension that ships one manifest
//   to both stores has. A .crx's ID comes from its key.
// - Loaded unpacked: when its manifest has Firefox's own key, gecko in
//   browser_specific_settings or in the older applications. Only a developer
//   loads one, and adds the key to get Firefox's behaviour.
bool IsFirefoxAddOn(const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_
