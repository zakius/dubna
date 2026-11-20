// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_DISCARDED_TAB_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_DISCARDED_TAB_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/common/extensions/api/tabs.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): tabs.create's discarded
// and title. Firefox opens a discarded tab without loading its page, shows
// the title until then, and loads the page when the user selects the tab.
// Chromium has such tabs too: a session restore's tabs that haven't loaded
// yet. So a discarded tab is made as one of them, and loads as they do.

// Firefox's error for `properties` that ask for a discarded tab Firefox
// doesn't make: an active or a pinned one, or one at the new tab page or an
// about: page (`url`, the validated URL, is the new tab page when
// `properties` has none), or for a title without discarded. nullopt if the
// properties are fine.
std::optional<std::string> FirefoxDiscardedTabError(
    const api::tabs::Create::Params::CreateProperties& properties,
    const GURL& url);

// The contents of a new tab at `url` with `title` that hasn't loaded: a
// restored tab with that one history entry, which loads its page when it is
// shown.
std::unique_ptr<content::WebContents> CreateFirefoxDiscardedTab(
    content::BrowserContext& browser_context,
    const GURL& url,
    const std::optional<std::string>& title);

// Whether Firefox calls `contents` discarded although Chromium doesn't: a tab
// whose page waits to load until the tab is shown, such as the tabs above and
// a session restore's. Chromium calls only the tabs that it unloaded
// discarded.
bool IsFirefoxDiscardedTab(content::WebContents& contents);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_DISCARDED_TAB_H_
