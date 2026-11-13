// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_
#define EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_

#include <string>

namespace extensions {

class Extension;
struct ActionInfo;

// Firefox add-ons (dubna's webextensions/README.md): Firefox lets an add-on
// have a "page_action" next to its "browser_action" or "action", and a
// "page_action" in Manifest V3. Chromium allows one action, and a page action
// only in Manifest V2. Such a page action is a second action: the extension
// action handler leaves it out of the add-on's one action, and pageAction.*
// calls reach it (ExtensionActionManager::GetFirefoxPageAction).

// Whether the manifest has a page action that is a second action.
bool HasFirefoxPageAction(const Extension& extension);

// Loads the second page action. Returns false and sets `error` when the
// manifest's "page_action" is invalid.
bool ParseFirefoxPageAction(Extension* extension, std::u16string* error);

// The second page action, or null.
const ActionInfo* GetFirefoxPageActionInfo(const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_
