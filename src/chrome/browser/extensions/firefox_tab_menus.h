// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/menu_manager.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the menus of a tab, as
// Firefox has them (ext-menus.js): the tab strip's menu of a tab and an
// add-on's toolbar button's menu, which opens on the active tab. Chrome Web
// Store extensions keep Chromium's.

// `filter`, the menu's, and for a Firefox add-on's item also its
// documentUrlPatterns, matched against the URL of `tab` (may be null) as the
// menu opens. Firefox shows such an item only on a tab that matches, also
// after a refresh() once the tab has navigated. Chromium matches them only
// in a page's menu, and shows the item on every tab.
base::RepeatingCallback<bool(const MenuItem*)> WithFirefoxDocumentUrl(
    base::RepeatingCallback<bool(const MenuItem*)> filter,
    content::WebContents* tab);

// The extensions with menus items, in the order of the tab strip's menu:
// Firefox add-ons in the order they were installed, as Firefox lists them,
// after the other extensions in the order of their IDs, as Chromium lists
// all of them (MenuManager::ExtensionIds()).
std::vector<MenuItem::ExtensionKey> FirefoxTabMenuExtensions(
    content::BrowserContext* context);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_
