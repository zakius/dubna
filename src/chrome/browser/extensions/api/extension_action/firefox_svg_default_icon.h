// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_SVG_DEFAULT_ICON_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_SVG_DEFAULT_ICON_H_

#include "base/values.h"

class ExtensionFunction;

namespace extensions {

// Firefox add-ons (dubna's webextensions/README.md): whether `function`, an
// action's setIcon() with `details`, sends the global icon that a Firefox
// add-on's background page drew from an SVG in the manifest
// (chrome/renderer/resources/extensions/firefox_svg_icons.js). A page action
// takes it too, though it has no global icon that an add-on can set.
bool IsFirefoxSvgDefaultIcon(const ExtensionFunction& function,
                             const base::DictValue* details);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_FIREFOX_SVG_DEFAULT_ICON_H_
