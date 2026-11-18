// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

// Firefox add-ons (dubna's webextensions/README.md): Firefox's pageAction
// functions that Chromium lacks.

namespace extensions {

// Whether show() showed the page action in the tab. Not a declarative rule's
// showing, which would tell the extension about pages it has no permission
// for, as action.isEnabled doesn't.
class PageActionIsShownFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.isShown", UNKNOWN)

 protected:
  ~PageActionIsShownFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_PAGE_ACTION_API_H_
