// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_

#include <string>
#include <vector>

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

// Sets `item`'s targetUrlPatterns (MenuItem::firefox_target_url_patterns()),
// matched as Firefox matches them for links (ext-menus.js, restrictSchemes
// false): a pattern of any scheme matches a link of that scheme, such as
// "magnet:?xt=*" or "javascript://-URL", and <all_urls> matches only the
// schemes Firefox supports. Chromium refuses "javascript://-URL", and its
// patterns match no magnet: or javascript: link. Returns false, with `error`,
// for a pattern that Firefox refuses too.
bool SetFirefoxTargetUrlPatterns(const std::vector<std::string>& patterns,
                                 MenuItem& item,
                                 std::string* error);

// Whether `item` shows for `params`. Firefox matches targetUrlPatterns against
// the link's or the medium's URL whatever the item's contexts, so an item
// with patterns shows only on a link or a medium that matches. Chromium reads
// them only in the link and media contexts, and an "all" item would show on
// every image and link. A link whose URL isn't valid (href="http://") is a
// link, as in Firefox, which no pattern matches; Chromium's menu has no link
// there. And text selected anywhere in the frame gives the selection context,
// as in Firefox, also on an image or a link next to it
// (ContextMenuParams::firefox_selection_text); Chromium's has it only on a
// click on the selection.
bool FirefoxMenuItemMatchesContext(const content::ContextMenuParams& params,
                                   const MenuItem& item);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ITEMS_H_
