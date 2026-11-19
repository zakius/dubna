// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
#define EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

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
  // ExtensionActionStorageManager still restores the add-on's own; a
  // transparent global colour counts as none, as an add-on's transparent one
  // does too.
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

  // The default icon that a Firefox add-on's background page drew from the
  // SVG in its manifest (chrome/renderer/resources/extensions/
  // firefox_svg_icons.js), or an empty image. It stands in for the manifest's
  // icon, which the browser can't decode, so an icon the add-on sets, for
  // the action, a window or a tab, shows over it, and clearing that one shows
  // it again. It isn't stored: each start of the background page draws it.
  static gfx::Image SvgDefaultIcon(const ExtensionAction& action);
  static void SetSvgDefaultIcon(ExtensionAction& action,
                                const gfx::Image& icon);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FIREFOX_ACTION_VALUES_H_
