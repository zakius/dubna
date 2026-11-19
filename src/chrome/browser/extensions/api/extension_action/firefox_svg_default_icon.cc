// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_svg_default_icon.h"

#include <string_view>

#include "extensions/browser/extension_function.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

bool IsFirefoxSvgDefaultIcon(const ExtensionFunction& function,
                             const base::DictValue* details) {
  return std::string_view(function.name()).ends_with(".setIcon") && details &&
         details->FindBool("firefoxSvgDefaultIcon").value_or(false) &&
         !details->contains("tabId") && !details->contains("windowId") &&
         function.extension() && IsFirefoxAddOn(*function.extension());
}

}  // namespace extensions
