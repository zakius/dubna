// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_
#define EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_

#include <optional>
#include <string>

class GURL;

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

// Loads the second page action, if there is one, and the page action's
// show_matches. Returns false and sets `error` when the manifest's
// "page_action" is invalid.
bool ParseFirefoxPageAction(Extension* extension, std::u16string* error);

// The second page action, or null.
const ActionInfo* GetFirefoxPageActionInfo(const Extension& extension);

// Firefox's "show_matches" and "hide_matches" of a "page_action", for the
// second page action or Chromium's: the page action shows on a page that
// show_matches matches and hide_matches doesn't, until show() or hide() for
// its tab, and each navigation of the tab matches again
// (chrome/browser/extensions/firefox_show_matches.h). Each is a list of match
// patterns of http, https, ws, wss, file or ftp, and show_matches may have
// <all_urls>, which matches data: too, as in Firefox. Without show_matches the
// page action shows only after show(), whatever hide_matches has.

// Whether the manifest's page action has show_matches.
bool HasFirefoxShowMatches(const Extension& extension);

// Whether the page action shows on `url` by its show_matches and
// hide_matches, or nullopt without show_matches.
std::optional<bool> MatchFirefoxShowMatches(const Extension& extension,
                                            const GURL& url);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_PAGE_ACTION_H_
