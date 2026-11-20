// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_

#include <stdint.h>

#include <string>
#include <string_view>

namespace bookmarks {
class BookmarkModel;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): reads `id_string`, an ID
// that the bookmarks API was given by `extension`, into `id`. It is Chromium's
// number, or one of Firefox's fixed folder IDs, which Firefox add-ons pass as
// literals: "root________" is the root, "toolbar_____" the bookmarks bar,
// "menu________" and "unfiled_____" Other bookmarks (Chromium has no bookmarks
// menu), and "mobile______" Mobile bookmarks, which Chromium's desktop hides,
// and the API refuses, while it is empty. A folder that the account has is the
// account's, as where bookmarks.create() puts a bookmark without a parent.
// A Firefox add-on sees these folders only by Firefox's IDs
// (ToFirefoxBookmarkId()), so for it their numbers are no node's ID and find
// nothing, as "000000000000" finds nothing in Firefox, where Chromium reads it
// as the root's 0.
bool ParseBookmarkId(const Extension* extension,
                     const bookmarks::BookmarkModel& model,
                     std::string_view id_string,
                     int64_t* id);

// Firefox add-ons: replaces `id_string`, a node's ID in what the bookmarks API
// gives a Firefox add-on, with Firefox's ID when the node is one of the folders
// above. Other bookmarks is "unfiled_____", Firefox's folder for a bookmark
// without a parent, which Firefox calls Other Bookmarks too.
void ToFirefoxBookmarkId(const bookmarks::BookmarkModel& model,
                         std::string* id_string);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_
