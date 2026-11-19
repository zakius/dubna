// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/tabs/firefox_current_window.h"

#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

namespace extensions {

mojom::EventFilteringInfoPtr FirefoxCurrentWindowInfo(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Its incognito profile when the add-on may see incognito windows.
  Profile* incognito = nullptr;
  if (util::CanCrossIncognito(extension, browser_context) &&
      profile->HasPrimaryOTRProfile()) {
    incognito = profile->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  }
  auto info = mojom::EventFilteringInfo::New();
  // As windows.getLastFocused() finds it: like Firefox's top window, never a
  // DevTools window.
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        WindowController* window =
            BrowserExtensionWindowController::From(browser);
        bool seen = browser->GetProfile() == profile ||
                    browser->GetProfile() == incognito;
        if (!window || !seen ||
            !window->IsVisibleToTabsAPIForExtension(
                extension, /*include_dev_tools_windows=*/false)) {
          return true;
        }
        info->firefox_current_window_id = window->GetWindowId();
        return false;
      });
  return info;
}

}  // namespace extensions
