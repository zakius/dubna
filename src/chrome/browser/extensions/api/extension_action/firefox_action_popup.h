// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_POPUP_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_POPUP_H_

#include <string_view>

#include "url/gurl.h"

class ExtensionFunction;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the URL of the popup that
// `function`, an action's setPopup(), sets. Firefox resolves `popup` against
// the URL of the page that calls it, so that "popup.html" from
// "data/background.html" is "data/popup.html"; Chromium, and so a Chrome
// extension, resolves it against the extension's root. Either way the URL
// must be in the extension, or it is empty.
GURL ResolveFirefoxPopupUrl(const ExtensionFunction& function,
                            std::string_view popup);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_POPUP_H_
