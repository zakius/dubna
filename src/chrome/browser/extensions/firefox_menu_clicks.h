// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_

namespace content {
class WebContents;
}

namespace extensions {

class Extension;
class MenuItem;

// Firefox add-ons (dubna's webextensions/README.md): a click on `item` of
// `extension` in `web_contents`'s menu, when the item's command
// (MenuItem::firefox_command()) is one that Firefox runs instead of firing
// onClicked. _execute_browser_action in Manifest V2 and _execute_action in
// Manifest V3 run the toolbar action as its keyboard shortcut does: its popup
// opens, or action.onClicked fires when it has none. _execute_page_action
// and _execute_sidebar_action do nothing until this browser has the page
// action's button and the sidebar (zakius/oganesson#68, #86). Returns false
// for any other item, extension or command: the caller fires onClicked, as
// Firefox does for a command it doesn't know.
bool RunFirefoxMenuCommand(const Extension* extension,
                           const MenuItem& item,
                           content::WebContents* web_contents);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_
