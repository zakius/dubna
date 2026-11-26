// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#ifndef EXTENSIONS_COMMON_FIREFOX_FILE_PATHS_H_
#define EXTENSIONS_COMMON_FIREFOX_FILE_PATHS_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace base {
class FilePath;
}

namespace extensions {

class Extension;
class ManifestHandler;
struct InstallWarning;

// Firefox add-ons (dubna's webextensions/README.md): Firefox reads a file path
// of the manifest as a URL, and loads an add-on whose manifest names a file
// that the add-on doesn't have. It shows no icon for a missing icon and a blank
// sidebar for a missing panel. Chromium refuses such an add-on.

// `path` as a Firefox add-on's manifest means it: without the spaces around
// it, which a URL drops (" icon.png" is icon.png). Another extension's `path`
// stays as it is.
std::string_view FirefoxResourcePath(const Extension& extension,
                                     std::string_view path);

// Called when `handler` refuses `extension` (ManifestHandler::Validate). When
// the handler checks the files of icons, actions or the side panel and
// `extension` is a Firefox add-on, moves `error` to `warnings` and returns
// true: the add-on loads. The browser shows the default icon for an icon file
// that the add-on doesn't have.
bool FirefoxLoadsWithoutFile(const Extension& extension,
                             const ManifestHandler& handler,
                             std::string* error,
                             std::vector<InstallWarning>* warnings);

// Removes from `image_paths`, the images that the installer decodes, those
// that a Firefox add-on at `extension_root` doesn't have or that are empty, as
// FirefoxLoadsWithoutFile() lets it load without them.
void RemoveMissingFirefoxImages(const Extension& extension,
                                const base::FilePath& extension_root,
                                std::set<base::FilePath>* image_paths);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FIREFOX_FILE_PATHS_H_
