// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CURRENT_WINDOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CURRENT_WINDOW_H_

#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): Firefox's filter of
// tabs.onUpdated takes windows.WINDOW_ID_CURRENT as its windowId, which
// Firefox's ext-tabs.js resolves at each update to its top window: the last
// focused browser window, a private one only for an add-on allowed in private
// windows, whichever context added the listener. The renderer checks the
// filter (firefox_update_filter.h) but doesn't know the windows, so the
// browser sends that window with each update to a Firefox add-on, in the
// event's filtering info. Null for another extension, whose event keeps its
// own.
mojom::EventFilteringInfoPtr FirefoxCurrentWindowInfo(
    content::BrowserContext* browser_context,
    const Extension* extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CURRENT_WINDOW_H_
