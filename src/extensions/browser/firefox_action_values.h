// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
#define EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_

#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

class Extension;
class ExtensionAction;

// Firefox add-ons (dubna's webextensions/README.md): an action's values as
// Firefox gives them.
class FirefoxActionValues {
 public:
  // Gives a Firefox add-on (extensions/common/firefox_add_on.h) Firefox's
  // default badge background, [217, 0, 0, 255]. Chromium has none, and paints
  // a grey badge.
  static void SetDefaults(ExtensionAction& action, const Extension& extension);

  // The badge text colour that Firefox gives a Firefox add-on: the colour set
  // for the tab, its window or globally, or else black or white, whichever
  // contrasts more with the badge's background. Chromium gives transparent
  // black, and paints a colour of its own.
  static SkColor BadgeTextColor(const ExtensionAction& action,
                                const Extension& extension,
                                int tab_id);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
