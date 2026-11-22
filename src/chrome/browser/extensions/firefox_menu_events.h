// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_EVENTS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_EVENTS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class ContextMenuMatcher;

// Firefox add-ons (dubna's webextensions/README.md): menus.onShown,
// menus.onHidden and menus.refresh(), as Firefox's ext-menus.js has them.
// Chromium has none of the three. A menu that shows extension items (a
// page's context menu, an add-on's toolbar button's menu and a tab's menu)
// tells its ContextMenuMatcher where it put them, when it opens and when it
// closes (ContextMenuMatcher::FirefoxItemsAppended() and the others).
//
// As the menu opens, onShown goes to each Firefox add-on that listens to it,
// or in an action menu to the action's add-on only, with the IDs of its items
// that the menu shows and, for an add-on with the host permissions of the
// page (or activeTab on the tab), the page's URLs and text, and with
// Firefox's menus permission the targetElementId of the element that a
// page's menu opened on (firefox_menu_target.h). As it closes, after a
// click on an item has fired onClicked, onHidden goes to those add-ons and
// to those whose items it showed. While it is open, refresh() rebuilds the
// calling add-on's items in it, where they were. Other extensions get no
// events.
//
// What a ContextMenuMatcher keeps of its menu for this.
struct FirefoxMenuState {
  explicit FirefoxMenuState(ContextMenuMatcher& matcher);
  FirefoxMenuState(const FirefoxMenuState&) = delete;
  FirefoxMenuState& operator=(const FirefoxMenuState&) = delete;
  // Sends onHidden if the menu is open, for a menu destroyed before it
  // closed.
  ~FirefoxMenuState();

  // The menu is open: the one that refresh() rebuilds, as Firefox has one.
  void Open(content::BrowserContext* context);
  // The menu closed: onHidden.
  void Close();

  raw_ptr<ContextMenuMatcher> matcher;

  // The extension items' place in the menu's model, [start, end), and what
  // ContextMenuMatcher::AppendExtensionItems() appended them with.
  size_t start = 0;
  size_t end = 0;
  std::u16string selection_text;
  bool is_action_menu = false;

  // An action menu's add-on, the only one whose items it shows.
  ExtensionId action_extension_id;

  bool open = false;
  raw_ptr<content::BrowserContext> browser_context = nullptr;
  // The Firefox add-ons that get onHidden.
  std::set<ExtensionId> shown_to;
};

// menus.refresh() of `extension_id`: rebuilds its items in the open menu, if
// that menu is of `context`'s profile.
void RefreshFirefoxMenu(content::BrowserContext* context,
                        const ExtensionId& extension_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_EVENTS_H_
