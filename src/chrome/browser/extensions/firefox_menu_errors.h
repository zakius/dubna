// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ERRORS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ERRORS_H_

#include <string>
#include <string_view>

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): where Firefox refuses a
// menus.create or update as Chromium does, a Firefox add-on gets Firefox's
// error text (ext-menus.js), which add-ons and Firefox's tests compare. Each
// Set function replaces Chromium's text in `error` when `extension` is a
// Firefox add-on, and leaves it otherwise. `extension` is null for a
// <webview>'s menu.

// Firefox's text for a pattern of documentUrlPatterns or targetUrlPatterns
// that it refuses; Chromium's is kInvalidURLPatternError, "Invalid url pattern
// '*'".
inline constexpr char kFirefoxInvalidUrlPatternError[] =
    "Invalid url pattern: *";

// Firefox's text for menus.create without an id, where it keeps the items
// (firefox_menu_store.h).
inline constexpr char kFirefoxMenuIdRequiredError[] =
    "menus.create requires an id for non-persistent background scripts.";

// menus.create with `id`, which an item of the add-on has. Firefox's text
// depends on its background: one that isn't persistent (an event page, or any
// Manifest V3 background) has its ids checked first, as it keeps the items.
void SetFirefoxDuplicateMenuIdError(const Extension* extension,
                                    std::string_view id,
                                    std::string* error);

// menus.update with a parentId of the item or of one of its descendants,
// which MenuManager::ChangeParent() refuses without a text, so the add-on gets
// "Unknown error.". Only an empty `error` is replaced.
void SetFirefoxMenuParentError(const Extension* extension, std::string* error);

// An invalid pattern in documentUrlPatterns, whose text is URLPatternSet's
// kInvalidURLPatternError. A Firefox add-on's targetUrlPatterns are checked by
// SetFirefoxTargetUrlPatterns(), with Firefox's text already.
void SetFirefoxUrlPatternError(const Extension* extension, std::string* error);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_MENU_ERRORS_H_
