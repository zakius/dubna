// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_STORE_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): the menus items of an
// add-on whose background Firefox runs as an event page, a Manifest V3
// background page or a Manifest V2 one with "persistent": false, stay across
// a restart, a disable and enable, and a reinstall of the same version; an
// update to another version or an uninstall drops them (ExtensionMenus.sys.mjs).
// Many such add-ons create their items only in runtime.onInstalled. Chromium
// runs a Manifest V3 background page as a persistent page and keeps none of
// its items, and keeps an event page's in its state store, which forgets them
// at every install, as at each start of a --load-extension add-on. So
// MenuManager keeps these add-ons' items here instead: in the add-on's
// extension prefs, which an uninstall removes, with the version that wrote
// them.

// Whether MenuManager keeps `extension`'s items here.
bool KeepsFirefoxMenus(const Extension& extension);

// Keeps `items`, all the items of the add-on `extension_id`, parents before
// their children, when it is one whose items are kept here. Returns whether it
// is.
bool WriteFirefoxMenus(content::BrowserContext* context,
                       const ExtensionId& extension_id,
                       const MenuItem::List& items);

// The items kept for `extension` at its version, as the list that
// MenuManager::ReadFromStorage() reads, or nullopt when there are none.
std::optional<base::Value> ReadFirefoxMenus(content::BrowserContext* context,
                                            const Extension& extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_STORE_H_
