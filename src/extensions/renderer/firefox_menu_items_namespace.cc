// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/firefox_menu_items_namespace.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"

namespace extensions {

bool IsFirefoxMenusNamespaceHidden(const Extension& extension,
                                   std::string_view api_name) {
  if (api_name != "menus" && api_name != "contextMenus") {
    return false;
  }
  if (!IsFirefoxAddOn(extension)) {
    return false;
  }
  const base::ListValue* permissions =
      extension.manifest()->value()->FindList("permissions");
  if (!permissions) {
    return false;
  }
  bool menus = permissions->contains("menus");
  bool context_menus = permissions->contains("contextMenus");
  if (api_name == "menus") {
    return context_menus && !menus;
  }
  return menus && !context_menus;
}

}  // namespace extensions
