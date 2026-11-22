// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_MENUS_PERMISSION_H_
#define EXTENSIONS_COMMON_FIREFOX_MENUS_PERMISSION_H_

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): whether `extension` is
// a Firefox add-on that asks for Firefox's "menus" permission in its
// manifest's permissions. This browser reads "menus" as "contextMenus" (an
// alias), but Firefox gives menus.getTargetElement and the menus'
// targetElementId only with "menus", not with "contextMenus" alone.
bool HasFirefoxMenusPermission(const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_MENUS_PERMISSION_H_
