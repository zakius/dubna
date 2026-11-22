// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ACCESS_KEYS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ACCESS_KEYS_H_

#include <map>
#include <string>
#include <string_view>

#include "chrome/browser/extensions/menu_manager.h"

namespace ui {
class SimpleMenuModel;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the access keys of a
// Firefox add-on's menus items, as Firefox gives them (ext-menus.js'
// customizeElement() and auto-accesskey.mjs). Chromium's menus mark an
// access key with an & in the item's title, which these produce.

// A menus item's title as Firefox reads it: an & marks the character after
// it as the access key, the first such & only, "&&" is an &, and the other
// &s go. "&%s" marks none: Chromium's menu would take the selection's first
// character.
struct FirefoxMenuLabel {
  std::u16string label;
  // Empty when the title marks none.
  std::u16string access_key;
};
FirefoxMenuLabel ParseFirefoxMenuTitle(std::u16string_view title);

// `label` as a Chromium menu's title: its &s doubled, and an & before the
// first `access_key` in it, as Firefox underlines that one, or else the key
// after it in parentheses, as Firefox shows a key that the label hasn't.
std::u16string FirefoxMenuTitle(std::u16string_view label,
                                std::u16string_view access_key);

// Gives each entry of a Firefox add-on's item in `model` (one menu, without
// its submenus) whose title marks no access key one: the first letter or
// digit of its label that a key press can type (not one of a script typed
// through an IME, such as Chinese, Japanese or Korean), or else the next
// digit that the menu's other entries don't use. When the menu's other
// entries have no access keys, as an action's menu's, the add-on's get none
// either. `items` are the menu's extension items, by command ID. Chromium
// gives an item whose title has no & no access key.
void AssignFirefoxAccessKeys(ui::SimpleMenuModel& model,
                             const std::map<int, MenuItem::Id>& items,
                             const MenuManager& menu_manager);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ACCESS_KEYS_H_
