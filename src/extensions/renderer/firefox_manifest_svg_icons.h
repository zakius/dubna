// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_RENDERER_FIREFOX_MANIFEST_SVG_ICONS_H_
#define EXTENSIONS_RENDERER_FIREFOX_MANIFEST_SVG_ICONS_H_

namespace extensions {

class ScriptContext;

// Firefox add-ons (dubna's webextensions/README.md): when `context` is a
// Firefox add-on's background page, draws the actions' default icons that are
// SVG files and sends them to the browser
// (chrome/renderer/resources/extensions/firefox_svg_icons.js). The browser
// decodes only raster images, so without this an SVG action icon shows as
// the placeholder.
void DrawFirefoxManifestSvgIcons(ScriptContext& context);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FIREFOX_MANIFEST_SVG_ICONS_H_
