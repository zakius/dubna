// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_VIEW_TYPES_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_VIEW_TYPES_H_

#include <vector>

#include "chrome/common/extensions/api/context_menus.h"

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class MenuItem;

// Firefox add-ons (dubna's webextensions/README.md): menus items' viewTypes
// and the viewType of a menu, as Firefox has them (ext-menus.js'
// getContextViewType() and enabledForContext()). A page's menu has the
// viewType of the page's view: "tab" in a tab, "popup" in an add-on's popup
// and "sidebar" in its sidebar, this browser's side panel. A tab's menu and
// a toolbar button's have none. A Firefox add-on's item created or updated
// with viewTypes shows only in the menus of those views, so never in a tab's
// or a toolbar button's menu; one without shows in any menu, as Firefox's
// "any view, including those without a viewType". onShown's and onClicked's
// info have the menu's viewType (firefox_menu_events.cc,
// firefox_menu_clicks.cc). Chromium has neither, and refuses viewTypes.

// Firefox's viewType of a menu of `web_contents`'s page, or null for a page
// in another view, such as an app's window.
const char* FirefoxMenuViewType(content::WebContents* web_contents);

// Sets `item`'s viewTypes (MenuItem::firefox_view_types()) to menus.create's
// or update's.
void SetFirefoxMenuViewTypes(
    const std::vector<api::context_menus::ViewType>& view_types,
    MenuItem& item);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_VIEW_TYPES_H_
