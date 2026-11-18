// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
#define EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_

#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

class Extension;
class ExtensionAction;

// Firefox add-ons (dubna's webextensions/README.md): an action's values as
// Firefox gives them. ExtensionAction makes this class a friend, so that it
// can read and clear one value of a tab, a window or the global one.
class FirefoxActionValues {
 public:
  // The values that Firefox's null clears.
  enum class Value {
    kTitle,
    kPopup,
    kIcon,
    kBadgeBackgroundColor,
    kBadgeTextColor,
  };

  // Clears `value` at `key`, a tab's or a window's ID or kDefaultTabId, as
  // Firefox's null does: a tab's or a window's value goes, so that the next
  // level's shows, and the global value goes back to the one in `defaults`, an
  // action made from the manifest.
  static void Clear(ExtensionAction& action,
                    const ExtensionAction& defaults,
                    Value value,
                    int key);

  // The badge background of the tab: for a Firefox add-on
  // (extensions/common/firefox_add_on.h) with none set for the tab, its window
  // or globally, Firefox's default, [217, 0, 0, 255]. Chromium has none, and
  // paints a grey badge. The default isn't a global value, so that
  // ExtensionActionStorageManager still restores the add-on's own.
  static SkColor BadgeBackgroundColor(const ExtensionAction& action,
                                      const Extension& extension,
                                      int tab_id);

  // The badge text colour that Firefox gives a Firefox add-on: the colour set
  // for the tab, its window or globally, or else black or white, whichever
  // contrasts more with BadgeBackgroundColor(). Chromium gives transparent
  // black, and paints a colour of its own.
  static SkColor BadgeTextColor(const ExtensionAction& action,
                                const Extension& extension,
                                int tab_id);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
