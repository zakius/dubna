// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/firefox_page_action_buttons.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace extensions {

DEFINE_USER_DATA(FirefoxPageActionButtons);

// static
FirefoxPageActionButtons* FirefoxPageActionButtons::From(
    BrowserWindowInterface& browser) {
  return Get(browser.GetUnownedUserDataHost());
}

}  // namespace extensions
