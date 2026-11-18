// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_MENU_ITEMS_NAMESPACE_H_
#define EXTENSIONS_RENDERER_FIREFOX_MENU_ITEMS_NAMESPACE_H_

#include <string_view>

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): whether the bindings
// leave out the namespace `api_name` for `extension`. Firefox gives
// browser.menus only with the "menus" permission and browser.contextMenus
// only with "contextMenus", and an add-on may check browser.menus to use
// Firefox's own calls. Here "menus" is an alias of "contextMenus", so both
// namespaces are the same API, and only the manifest's permissions tell
// which one the add-on asked for. Chrome Web Store extensions, and an add-on
// that lists neither (it may ask for one at run time), keep both.
bool IsFirefoxMenusNamespaceHidden(const Extension& extension,
                                   std::string_view api_name);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_MENU_ITEMS_NAMESPACE_H_
