// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_menus_permission.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

bool HasFirefoxMenusPermission(const Extension& extension) {
  if (!IsFirefoxAddOn(extension)) {
    return false;
  }
  const base::ListValue* permissions =
      extension.manifest()->value()->FindList(manifest_keys::kPermissions);
  if (!permissions) {
    return false;
  }
  for (const base::Value& permission : *permissions) {
    if (permission.is_string() && permission.GetString() == "menus") {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
