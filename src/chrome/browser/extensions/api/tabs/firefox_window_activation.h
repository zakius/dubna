// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_WINDOW_ACTIVATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_WINDOW_ACTIVATION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

class BrowserWindowInterface;

namespace extensions {

class Extension;
class WindowController;

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// windows.create() resolves with a focused window that is the top window, the
// current window of tabs.query({currentWindow: true}) and the other APIs. For
// a background page, Chromium's current window is the first in its order of
// activation, which Show() gives the new window. But when windows.create()
// moves the last tab of a window, that window closes, and its native window
// goes a few ms later. On X11, the window system can then activate another
// browser window for a moment, while the new window's widget stays the active
// one: the other window takes the lead in the order and keeps it, since the
// new window isn't activated again (zakius/oganesson#151).
//
// So the response to a Firefox add-on's windows.create() that empties a
// window and shows the new one active waits until the emptied window is gone,
// and a moment more for that activation, at most a second. If another browser
// became active meanwhile while the new window's widget was active, and the
// new window is still active but no longer first in the order, it is made the
// last active again before the response.
class FirefoxWindowActivation {
 public:
  // `source_window` holds the tab that windows.create() moves, or is null.
  FirefoxWindowActivation(const Extension* extension,
                          WindowController* source_window);
  FirefoxWindowActivation(const FirefoxWindowActivation&) = delete;
  FirefoxWindowActivation& operator=(const FirefoxWindowActivation&) = delete;
  ~FirefoxWindowActivation();

  // Whether the response to the creation of `new_window`, shown and alive,
  // has to wait.
  bool Waits(BrowserWindowInterface* new_window);

  // After Waits() returned true: runs `respond` once the wait is over.
  void RespondLater(base::OnceClosure respond);

 private:
  // Whether a Firefox add-on's windows.create() moves a tab of a browser
  // window.
  bool moves_tab_ = false;
  base::WeakPtr<BrowserWindowInterface> source_window_;
  base::WeakPtr<BrowserWindowInterface> new_window_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_WINDOW_ACTIVATION_H_
