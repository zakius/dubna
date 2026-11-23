// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_

#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "chrome/browser/extensions/menu_manager.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class ExtensionAction;

// Firefox add-ons (dubna's webextensions/README.md): the menus of a tab, as
// Firefox has them (ext-menus.js): the tab strip's menu of a tab and an
// add-on's toolbar button's menu, which opens on the active tab. Chrome Web
// Store extensions keep Chromium's.

// `filter`, the menu's, and for a Firefox add-on's item also its
// documentUrlPatterns, matched against the URL of `tab` (may be null) as the
// menu opens. Firefox shows such an item only on a tab that matches, also
// after a refresh() once the tab has navigated. Chromium matches them only
// in a page's menu, and shows the item on every tab. A Firefox add-on's item
// with targetUrlPatterns never shows in these menus, which have no link or
// medium for them to match, as in Firefox (enabledForContext()).
base::RepeatingCallback<bool(const MenuItem*)> WithFirefoxDocumentUrl(
    base::RepeatingCallback<bool(const MenuItem*)> filter,
    content::WebContents* tab);

// The extensions with menus items, in the order of the tab strip's menu:
// Firefox add-ons in the order they were installed, as Firefox lists them,
// after the other extensions in the order of their IDs, as Chromium lists
// all of them (MenuManager::ExtensionIds()).
std::vector<MenuItem::ExtensionKey> FirefoxTabMenuExtensions(
    content::BrowserContext* context);

// While it lives, the button menu that an add-on's ExtensionContextMenuModel
// builds or shows is that of `action`'s button. A Firefox add-on's page action
// next to its other action has a button of its own (oganesson's location
// bar), whose menu shows the add-on's page_action items and reports
// page_action to menus.onShown, as Firefox's does.
class ScopedFirefoxMenuAction {
 public:
  explicit ScopedFirefoxMenuAction(ExtensionAction* action);
  ScopedFirefoxMenuAction(const ScopedFirefoxMenuAction&) = delete;
  ScopedFirefoxMenuAction& operator=(const ScopedFirefoxMenuAction&) = delete;
  ~ScopedFirefoxMenuAction();

 private:
  base::AutoReset<ExtensionAction*> reset_;
};

// The action of the button whose menu is built or shown: the scoped one, or
// else `primary`, the add-on's own action.
ExtensionAction* FirefoxMenuAction(ExtensionAction* primary);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_TAB_MENUS_H_
