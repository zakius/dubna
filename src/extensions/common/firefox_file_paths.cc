// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_file_paths.h"

#include <stdint.h>

#include <algorithm>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

namespace {

// The keys whose handlers check only that the files they name are there.
// "side_panel"'s handler reads Firefox's "sidebar_action" in oganesson.
constexpr std::string_view kKeysWithFiles[] = {
    manifest_keys::kIcons,         manifest_keys::kAction,
    manifest_keys::kBrowserAction, manifest_keys::kPageAction,
    "side_panel",
};

}  // namespace

std::string_view FirefoxResourcePath(const Extension& extension,
                                     std::string_view path) {
  std::string_view trimmed = base::TrimWhitespaceASCII(path, base::TRIM_ALL);
  if (trimmed.size() == path.size() || !IsFirefoxAddOn(extension)) {
    return path;
  }
  return trimmed;
}

bool FirefoxLoadsWithoutFile(const Extension& extension,
                             const ManifestHandler& handler,
                             std::string* error,
                             std::vector<InstallWarning>* warnings) {
  if (!IsFirefoxAddOn(extension)) {
    return false;
  }
  for (const char* key : handler.Keys()) {
    if (std::ranges::find(kKeysWithFiles, std::string_view(key)) !=
        std::end(kKeysWithFiles)) {
      warnings->emplace_back(*error);
      error->clear();
      return true;
    }
  }
  return false;
}

void RemoveMissingFirefoxImages(const Extension& extension,
                                const base::FilePath& extension_root,
                                std::set<base::FilePath>* image_paths) {
  if (!IsFirefoxAddOn(extension)) {
    return;
  }
  std::erase_if(*image_paths, [&](const base::FilePath& path) {
    std::optional<int64_t> size =
        base::GetFileSize(extension_root.Append(path));
    return size.value_or(0) == 0;
  });
}

}  // namespace extensions
