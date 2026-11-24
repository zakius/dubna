// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_

#include "base/values.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;
class MenuItem;

// Firefox add-ons (dubna's webextensions/README.md): a click on `item` of
// `extension` in `web_contents`'s menu, when the item's command
// (MenuItem::firefox_command()) is one that Firefox runs instead of firing
// onClicked. _execute_browser_action in Manifest V2 and _execute_action in
// Manifest V3 run the toolbar action as its keyboard shortcut does: its popup
// opens, or action.onClicked fires when it has none. _execute_page_action
// runs the page action as its shortcut does (firefox_page_action_buttons.h),
// and _execute_sidebar_action does nothing until this browser has the
// sidebar (#86). Returns false for any other item, extension or command: the
// caller fires onClicked, as Firefox does for a command it doesn't know.
bool RunFirefoxMenuCommand(const Extension* extension,
                           const MenuItem& item,
                           content::WebContents* web_contents);

// Firefox's part of a click's OnClickData for a Firefox add-on's item, added
// to `properties`: `modifiers`, the keys held in `event_flags` (Shift, Alt,
// Command, Ctrl, and MacCtrl on a Mac), `button`, the mouse button (0, 1 or
// 2), and `linkText`, the text of the link that the menu is for, and
// `linkUrl` for a link whose URL isn't valid. Its `selectionText` is the
// text selected anywhere in the frame, with the white space it starts at
// (" just some text"), where Chromium's is only for a click on the
// selection, without that space. Its `viewType` is the view of the page in
// `frame`, the frame of a page's menu (firefox_menu_view_types.h); a tab's
// or a toolbar button's menu has no frame and no viewType. Its
// `targetElementId`, for an add-on with Firefox's menus permission, is the
// ID of the element that a page's menu opened on, which
// menus.getTargetElement takes (firefox_menu_target.h).
void AddFirefoxClickData(const Extension* extension,
                         content::RenderFrameHost* frame,
                         const content::ContextMenuParams& params,
                         int event_flags,
                         base::DictValue& properties);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_CLICKS_H_
