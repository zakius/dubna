// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_menu_events_bindings.h"

#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

bool IsFirefoxMenusMemberHidden(const Extension* extension,
                                std::string_view name) {
  // browser.menus is the same binding as contextMenus, with these names.
  if (name != "contextMenus.onShown" && name != "contextMenus.onHidden" &&
      name != "contextMenus.refresh") {
    return false;
  }
  return !extension || !IsFirefoxAddOn(*extension);
}

}  // namespace extensions
