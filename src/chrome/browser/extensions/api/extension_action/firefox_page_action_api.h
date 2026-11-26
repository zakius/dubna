// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

// Firefox add-ons (dubna's webextensions/README.md): Firefox's pageAction
// functions that Chromium lacks.

namespace extensions {

// Whether show(), or else the manifest's show_matches, showed the page action
// in the tab (chrome/browser/extensions/firefox_show_matches.h), as in
// Firefox. Not a declarative rule's showing, which would tell the extension
// about pages it has no permission for, as action.isEnabled doesn't.
class PageActionIsShownFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.isShown", UNKNOWN)

 protected:
  ~PageActionIsShownFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// Firefox's pageAction.openPopup(): runs the page action in the active tab
// of the top window (the last focused browser window that the add-on may
// see) as a click on its button does, from a user action only, with
// Firefox's error otherwise: the popup opens at the button, or onClicked
// fires (firefox_page_action_buttons.h).
class PageActionOpenPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.openPopup", UNKNOWN)

 protected:
  ~PageActionOpenPopupFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_
