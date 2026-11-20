// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ICONS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ICONS_H_

#include <map>
#include <string>

#include "chrome/browser/extensions/menu_manager.h"

namespace content {
class BrowserContext;
}

namespace ui {
class SimpleMenuModel;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): a menus item's own icon,
// as Firefox shows it (ext-menus.js). An item created or updated with
// `icons`, Firefox's {size: path}, shows the image of the path that Firefox
// picks for 16 px next to its title, in an add-on's submenu and in an
// action's menu. Chromium shows no icon there. On the add-on's one item at
// the top of the page's menu, Firefox shows the add-on's icon instead, as
// Chromium does.

// Sets `item`'s icons, `extension`'s, to menus.create's or update's `icons`,
// {size: path}; the item shows the image once it has loaded. No icons
// (update's null, which context_menus_handlers.js sends as {}) removes them,
// as in Firefox. A Chrome Web Store extension's item gets none.
void SetFirefoxMenuItemIcons(content::BrowserContext* context,
                             const Extension& extension,
                             MenuItem& item,
                             const std::map<std::string, std::string>& icons);

// Loads the images of `extension`'s items that have icons, as MenuManager
// restores an event page's items from storage after a restart.
void LoadFirefoxMenuIcons(content::BrowserContext* context,
                          const Extension& extension);

// Shows `item`'s own icon, when it has one, on its entry `command_id` in
// `menu_model`.
void ShowFirefoxMenuItemIcon(const MenuItem& item,
                             int command_id,
                             ui::SimpleMenuModel& menu_model);

// Whether the page's menu shows `extension`'s icon on its top-level item:
// Firefox shows none for an add-on whose manifest has no icons, where
// Chromium shows a puzzle piece.
bool FirefoxShowsAddOnMenuIcon(const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ICONS_H_
