// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_USER_SETTINGS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_USER_SETTINGS_H_

#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): where Firefox puts an
// add-on's toolbar button, and the event that tells the add-on it moved.

// Fires browserAction.onUserSettingsChanged for a Firefox add-on with a
// browser_action that was pinned to the toolbar or unpinned, as Chromium's
// ExtensionActionDispatcher fires action.onUserSettingsChanged for an action.
void DispatchFirefoxBrowserActionUserSettingsChanged(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    bool is_pinned);

// Whether a new extension is pinned to the toolbar. A Firefox add-on's
// default_area in its browser_action or action decides, as in Firefox:
// "navbar", "tabstrip" and "personaltoolbar" pin it, "menupanel" doesn't.
// Without one, and for every other extension, `pinned_by_default`, the
// setting that pins new extensions, decides.
bool FirefoxPinnedAtInstall(const Extension& extension, bool pinned_by_default);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_ACTION_USER_SETTINGS_H_
