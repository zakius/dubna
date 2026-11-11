// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/common/firefox_version.h"

#include <memory>
#include <string_view>

#include "base/strings/string_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/version_name_info.h"

namespace extensions {

base::Version LoadFirefoxVersion(const std::string& version,
                                 Extension* extension) {
  size_t end = version.find_first_not_of("0123456789.");
  std::string_view numbers =
      base::TrimString(std::string_view(version).substr(0, end), ".",
                       base::TRIM_TRAILING);
  base::Version parsed(numbers);
  if (!parsed.IsValid() || parsed.components().size() > 4) {
    return base::Version();
  }
  // The version_name handler parses the manifest's own key later, and an
  // add-on has only one version name.
  if (!extension->manifest()->FindKey(manifest_keys::kVersionName)) {
    extension->SetManifestData(std::make_unique<VersionNameInfo>(version));
  }
  return parsed;
}

}  // namespace extensions
