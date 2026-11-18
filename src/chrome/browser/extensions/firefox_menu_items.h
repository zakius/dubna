// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_

#include "chrome/browser/extensions/menu_manager.h"

namespace content {
struct ContextMenuParams;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): menus items as Firefox
// makes them (ext-menus.js), for a Firefox add-on's items only
// (MenuItem::firefox_add_on()). Chrome Web Store extensions keep Chromium's.

// The contexts of an item created without any: its parent's, or else "all".
// Chromium gives every such item "page", so it is missing from the menu of an
// image, a link or a selection, and an action's or a tab's submenu shows
// without its children.
MenuItem::ContextList FirefoxDefaultMenuContexts(
    const MenuManager& menu_manager,
    const MenuItem::Id* parent_id);

// Whether `item` shows for `params`. Firefox matches targetUrlPatterns against
// the link's or the medium's URL whatever the item's contexts, so an item
// with patterns shows only on a link or a medium that matches. Chromium reads
// them only in the link and media contexts, and an "all" item would show on
// every image and link.
bool FirefoxMenuItemMatchesContext(const content::ContextMenuParams& params,
                                   const MenuItem& item);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_
