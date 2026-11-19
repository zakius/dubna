// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/extensions/api/extension_action/firefox_svg_default_icon.h"

#include <string_view>

#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/firefox_action_values.h"
#include "extensions/browser/icon_util.h"
#include "extensions/common/firefox_add_on.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

bool IsFirefoxSvgDefaultIcon(const ExtensionFunction& function,
                             const base::DictValue* details) {
  return std::string_view(function.name()).ends_with(".setIcon") && details &&
         details->FindBool("firefoxSvgDefaultIcon").value_or(false) &&
         !details->contains("tabId") && !details->contains("windowId") &&
         function.extension() && IsFirefoxAddOn(*function.extension());
}

bool SetFirefoxSvgDefaultIcon(ExtensionAction& action,
                              const base::DictValue& details) {
  const base::DictValue* image_data = details.FindDict("imageData");
  if (!image_data) {
    return false;
  }
  gfx::ImageSkia icon;
  IconParseResult result = ParseIconFromCanvasDictionary(*image_data, &icon);
  if (result != IconParseResult::kSuccess || icon.isNull()) {
    return false;
  }
  FirefoxActionValues::SetSvgDefaultIcon(action, gfx::Image(icon));
  return true;
}

}  // namespace extensions
