// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/firefox_action_values.h"

#include <cmath>

#include "extensions/browser/extension_action.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

namespace {

// A colour channel's share of the relative luminance (WCAG 2.0).
double Linear(U8CPU channel) {
  double value = channel / 255.0;
  if (value <= 0.03928) {
    return value / 12.92;
  }
  return std::pow((value + 0.055) / 1.055, 2.4);
}

}  // namespace

void FirefoxActionValues::SetDefaults(ExtensionAction& action,
                                      const Extension& extension) {
  if (IsFirefoxAddOn(extension)) {
    action.SetBadgeBackgroundColor(ExtensionAction::kDefaultTabId,
                                   SkColorSetARGB(255, 217, 0, 0));
  }
}

SkColor FirefoxActionValues::BadgeTextColor(const ExtensionAction& action,
                                            const Extension& extension,
                                            int tab_id) {
  SkColor color = action.GetBadgeTextColor(tab_id);
  // setBadgeTextColor() refuses a transparent colour, so a transparent one is
  // none.
  if (!IsFirefoxAddOn(extension) ||
      SkColorGetA(color) != SK_AlphaTRANSPARENT) {
    return color;
  }
  // Firefox's choice, from WCAG's contrast ratio: black has (L + 0.05) / 0.05,
  // white 1.05 / (L + 0.05).
  SkColor background = action.GetBadgeBackgroundColor(tab_id);
  double luminance = 0.2126 * Linear(SkColorGetR(background)) +
                     0.7152 * Linear(SkColorGetG(background)) +
                     0.0722 * Linear(SkColorGetB(background));
  if (1.05 * 0.05 < (luminance + 0.05) * (luminance + 0.05)) {
    return SK_ColorBLACK;
  }
  return SK_ColorWHITE;
}

}  // namespace extensions
