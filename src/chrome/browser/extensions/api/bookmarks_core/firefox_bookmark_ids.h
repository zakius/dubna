// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_

#include <stdint.h>

#include <string_view>

namespace bookmarks {
class BookmarkModel;
}

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): reads `id_string`, an ID
// that the bookmarks API was given, into `id`. It is Chromium's number, or one
// of Firefox's fixed folder IDs, which Firefox add-ons pass as literals:
// "root________" is the root, "toolbar_____" the bookmarks bar, "menu________"
// and "unfiled_____" Other bookmarks (Chromium has no bookmarks menu), and
// "mobile______" Mobile bookmarks, which Chromium's desktop hides, and the API
// refuses, while it is empty. A folder that the account has is the account's,
// as where bookmarks.create() puts a bookmark without a parent.
bool ParseBookmarkId(const bookmarks::BookmarkModel& model,
                     std::string_view id_string,
                     int64_t* id);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_IDS_H_
