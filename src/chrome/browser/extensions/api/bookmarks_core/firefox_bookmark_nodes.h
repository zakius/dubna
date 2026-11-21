// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_NODES_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_NODES_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/event_args.h"

namespace bookmarks {
class BookmarkModel;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox's bookmark nodes
// have a type, "bookmark" or "folder" (or "separator", which this browser
// doesn't have), in what the bookmarks API returns and in its events.
// Chromium's nodes have none, and a node without a URL is a folder. Firefox's
// fixed folders have Firefox's IDs there too, in `id`, `parentId` and the
// events' parent IDs (firefox_bookmark_ids.h). They keep this browser's
// titles, which the user sees in its bookmark bar and manager, as a Firefox
// add-on shows Firefox's titles in Firefox's language. Returns `results`,
// what a bookmarks function answers `extension`, with Firefox's nodes for a
// Firefox add-on, and as they are for another extension.
base::ListValue FirefoxBookmarkResults(const Extension* extension,
                                       const bookmarks::BookmarkModel& model,
                                       base::ListValue results);

// The same for the bookmarks event `event_name` with `event_args`: the
// callback that gives a Firefox add-on the event with Firefox's IDs and
// nodes.
Event::WillDispatchCallback FirefoxBookmarkEventCallback(
    const std::string& event_name,
    scoped_refptr<const EventArgs> event_args);

// Firefox's bookmarks.create() takes the type of the node: a "folder" has no
// URL, even when it was given one, and a "bookmark" needs one, where Chromium
// would make a folder. Clears `url` for a folder; sets `error` and returns
// false for a bookmark without a URL, or for another type. Any extension may
// pass the type, as the schema takes it from all of them. The schema has it as
// a plain string: a named enum type would show on every extension's
// chrome.bookmarks, and Chromium allows no new inline enums.
bool ApplyFirefoxBookmarkType(const std::optional<std::string>& type,
                              std::string* url,
                              std::string* error);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_CORE_FIREFOX_BOOKMARK_NODES_H_
