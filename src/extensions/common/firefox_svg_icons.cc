// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_svg_icons.h"

#include <set>

#include "extensions/common/firefox_add_on.h"

namespace extensions {

void EraseFirefoxSvgImagePaths(const Extension& extension,
                               std::set<base::FilePath>& image_paths) {
  if (!IsFirefoxAddOn(extension)) {
    return;
  }
  std::erase_if(image_paths, [](const base::FilePath& path) {
    return path.MatchesExtension(FILE_PATH_LITERAL(".svg"));
  });
}

}  // namespace extensions
