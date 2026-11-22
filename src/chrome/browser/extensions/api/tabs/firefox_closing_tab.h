// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CLOSING_TAB_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CLOSING_TAB_H_

#include "base/callback_list.h"
#include "chrome/browser/extensions/api/tabs/firefox_tab_status.h"
#include "components/tabs/public/tab_interface.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): a pinned tab that closes
// sends no tabs.onUpdated, as in Firefox. Chromium takes the tab out of its
// window's pinned tabs as it detaches it, and TabModel then reports the tab
// unpinned, which the router sends as "pinned: false". A tab that is detached
// to move to another window keeps that update.
class FirefoxClosingTab {
 public:
  // Starts to watch `tab` for its close.
  void Observe(tabs::TabInterface& tab);

  // The audience of the router's "pinned" update.
  FirefoxTabStatus::Audience PinnedAudience() const;

 private:
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  bool closing_ = false;
  base::CallbackListSubscription will_detach_subscription_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_FIREFOX_CLOSING_TAB_H_
