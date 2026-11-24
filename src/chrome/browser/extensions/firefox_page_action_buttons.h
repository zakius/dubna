// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_FIREFOX_PAGE_ACTION_BUTTONS_H_
#define CHROME_BROWSER_EXTENSIONS_FIREFOX_PAGE_ACTION_BUTTONS_H_

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): the buttons of Firefox
// add-ons' page actions in a browser window, which a browser that has them
// registers as the window's user data (oganesson's location bar,
// zakius/oganesson#68; tennessine has none yet). pageAction.openPopup() and
// the menus command _execute_page_action run a page action through them.
class FirefoxPageActionButtons {
 public:
  DECLARE_USER_DATA(FirefoxPageActionButtons);

  // The buttons of `browser`, or null.
  static FirefoxPageActionButtons* From(BrowserWindowInterface& browser);

  virtual ~FirefoxPageActionButtons() = default;

  // Runs `extension`'s page action in the window's active tab as its
  // _execute_page_action shortcut does: the add-on gets activeTab, then the
  // popup opens at the button, or pageAction.onClicked fires. A page action
  // hidden in the tab does nothing. False when the add-on has no page action
  // button in the window.
  virtual bool RunPageAction(const Extension& extension) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FIREFOX_PAGE_ACTION_BUTTONS_H_
