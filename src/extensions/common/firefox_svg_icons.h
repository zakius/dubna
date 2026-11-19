// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_SVG_ICONS_H_
#define EXTENSIONS_COMMON_FIREFOX_SVG_ICONS_H_

#include <set>

#include "base/files/file_path.h"

namespace extensions {

class Extension;

// Firefox add-ons (dubna's webextensions/README.md): when `extension` is a
// Firefox add-on, removes its SVG files from `image_paths`, the images that a
// packed install decodes and saves again as PNG files
// (ChromeExtensionsClient::GetBrowserImagePaths()). The decoder takes only
// raster images, so an SVG icon, which Firefox draws, would fail the whole
// install. The add-on's pages draw its SVG action icons instead
// (chrome/renderer/resources/extensions/firefox_svg_icons.js). The browser
// decodes only the images that the install saved again, so ImageLoader
// doesn't decode an .svg file, whatever it holds: a PNG named icon.svg
// would reach it unsanitized.
void EraseFirefoxSvgImagePaths(const Extension& extension,
                               std::set<base::FilePath>& image_paths);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_SVG_ICONS_H_
