// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_ACTION_WINDOW_H_
#define EXTENSIONS_BROWSER_FIREFOX_ACTION_WINDOW_H_

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's action setters
// and getters take a windowId as well as a tabId. A window's value holds for
// every tab of that window, under a tab's own value and over the global one.
// ExtensionAction keeps it in the same maps as the tabs' values, under the
// window's ID: windows and tabs take their IDs from one SessionID counter, so
// the two never clash. ExtensionAction::GetValue() finds a tab's window with
// FirefoxWindowOfTab().

// The ID of the window that has the tab `tab_id`. ExtensionAction::kDefaultTabId
// when no add-on has set a window's value yet, or no window has the tab.
int FirefoxWindowOfTab(int tab_id);

// Sets the function that finds a tab's window, which needs the browser's
// window list (chrome/browser/extensions/api/extension_action/
// firefox_action_window.h). It is set when an add-on first sets a window's
// value, so until then FirefoxWindowOfTab() costs nothing.
void SetFirefoxWindowOfTab(int (*window_of_tab)(int tab_id));

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FIREFOX_ACTION_WINDOW_H_
