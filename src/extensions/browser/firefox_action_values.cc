// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/browser/firefox_action_values.h"

#include <cmath>
#include <map>

#include "base/containers/map_util.h"

#include "extensions/browser/extension_action.h"
#include "extensions/browser/firefox_action_window.h"
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

template <class T>
void ClearValue(std::map<int, T>& values,
                const std::map<int, T>& defaults,
                int key) {
  values.erase(key);
  if (key != ExtensionAction::kDefaultTabId) {
    return;
  }
  if (const T* value = base::FindOrNull(defaults, key)) {
    values[key] = *value;
  }
}

}  // namespace

void FirefoxActionValues::Clear(ExtensionAction& action,
                                const ExtensionAction& defaults,
                                Value value,
                                int key) {
  switch (value) {
    case Value::kTitle:
      ClearValue(action.title_, defaults.title_, key);
      break;
    case Value::kPopup:
      ClearValue(action.popup_url_, defaults.popup_url_, key);
      break;
    case Value::kIcon:
      // The manifest's icon isn't in the map: without a value, it shows.
      ClearValue(action.icon_, defaults.icon_, key);
      break;
    case Value::kBadgeBackgroundColor:
      ClearValue(action.badge_background_color_,
                 defaults.badge_background_color_, key);
      break;
    case Value::kBadgeTextColor:
      ClearValue(action.badge_text_color_, defaults.badge_text_color_, key);
      break;
  }
}

SkColor FirefoxActionValues::BadgeBackgroundColor(
    const ExtensionAction& action,
    const Extension& extension,
    int tab_id) {
  const std::map<int, SkColor>& values = action.badge_background_color_;
  // ExtensionActionStorageManager saves an unset global colour as transparent
  // black, and restores it after a restart, so that counts as none.
  const SkColor* global =
      base::FindOrNull(values, ExtensionAction::kDefaultTabId);
  if (!IsFirefoxAddOn(extension) || values.contains(tab_id) ||
      values.contains(FirefoxWindowOfTab(tab_id)) ||
      (global && SkColorGetA(*global) != SK_AlphaTRANSPARENT)) {
    return action.GetBadgeBackgroundColor(tab_id);
  }
  return SkColorSetARGB(255, 217, 0, 0);
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
  SkColor background = BadgeBackgroundColor(action, extension, tab_id);
  double luminance = 0.2126 * Linear(SkColorGetR(background)) +
                     0.7152 * Linear(SkColorGetG(background)) +
                     0.0722 * Linear(SkColorGetB(background));
  if (1.05 * 0.05 < (luminance + 0.05) * (luminance + 0.05)) {
    return SK_ColorBLACK;
  }
  return SK_ColorWHITE;
}

}  // namespace extensions
