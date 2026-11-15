// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_
#define EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): whether `extension` is
// written for Firefox, that is, its manifest has Firefox's own key, gecko in
// browser_specific_settings or in the older applications. Where Firefox and
// Chromium give the same thing a different value, a Firefox add-on gets
// Firefox's, and an extension without the key keeps Chromium's. So a Chrome
// Web Store extension that ships one manifest for both stores counts as a
// Firefox add-on, and an AMO add-on without the key, which AMO allows in
// Manifest V2, doesn't.
bool IsFirefoxAddOn(const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_ADD_ON_H_
