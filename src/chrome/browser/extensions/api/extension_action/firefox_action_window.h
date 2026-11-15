// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_WINDOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_WINDOW_H_

#include <string>

#include "base/values.h"

class ExtensionFunction;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): the windowId of
// Firefox's action setters and getters (extensions/browser/
// firefox_action_window.h).

// Whether the call's `details` have a windowId. If so, `key` becomes that
// window's ID, under which the call sets or gets the window's value, or `error`
// says why the call fails: there is no such window, or the details have a
// tabId too, which Firefox refuses.
bool TakeFirefoxWindowId(ExtensionFunction& function,
                         const base::DictValue* details,
                         int& key,
                         std::string& error);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_ACTION_WINDOW_H_
