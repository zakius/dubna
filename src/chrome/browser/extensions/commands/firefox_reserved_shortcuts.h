// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_COMMANDS_FIREFOX_RESERVED_SHORTCUTS_H_
#define CHROME_BROWSER_EXTENSIONS_COMMANDS_FIREFOX_RESERVED_SHORTCUTS_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): whether a Firefox
// add-on's suggested key may take `accelerator` when it is one of the
// browser's own shortcuts, as in Firefox: every one but those of the
// commands Chromium reserves (IsReservedCommand(): closing, opening and
// reopening tabs and windows, switching tabs, quitting, DevTools, tab search,
// copying the URL), which a web page can't take either, and F11, which leaves
// full screen. CommandService gives such a command no key, as in Chromium.
bool FirefoxMayTakeBrowserShortcut(const ui::Accelerator& accelerator);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMMANDS_FIREFOX_RESERVED_SHORTCUTS_H_
