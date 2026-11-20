// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_FIREFOX_CONTEXT_MENUS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_FIREFOX_CONTEXT_MENUS_API_H_

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): Firefox's
// menus.refresh(), which rebuilds the add-on's items in the open menu
// (firefox_menu_events.h). It does nothing while no menu is open.
class ContextMenusRefreshFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.refresh", UNKNOWN)

 protected:
  ~ContextMenusRefreshFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_FIREFOX_CONTEXT_MENUS_API_H_
