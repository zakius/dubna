// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_svg_icons.h"

#include <set>

#include "extensions/common/extension.h"
#include "extensions/common/firefox_add_on.h"

namespace extensions {

void EraseFirefoxSvgImagePaths(const Extension& extension,
                               std::set<base::FilePath>& image_paths) {
  // A theme's images the browser decodes itself (BrowserThemePack), so a
  // theme's SVG files stay, and its install still fails on them.
  if (!IsFirefoxAddOn(extension) || extension.is_theme()) {
    return;
  }
  std::erase_if(image_paths, [](const base::FilePath& path) {
    return path.MatchesExtension(FILE_PATH_LITERAL(".svg"));
  });
}

}  // namespace extensions
